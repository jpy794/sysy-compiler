#include "loop_unroll.hh"
#include "log.hh"
#include <codecvt>
#include <type_traits>

using namespace pass;
using namespace ir;
using namespace std;

optional<LoopUnroll::SimpleLoopInfo>
LoopUnroll::parse_simple_loop(BasicBlock *header, const LoopInfo &loop) {
    SimpleLoopInfo ret;

    ret.bbs = loop.bbs;

    // the loop has too many bbs
    if (loop.bbs.size() > 2) {
        return nullopt;
    }

    // parse header
    ret.header = header;

    // parse body
    for (auto &&bb : loop.bbs) {
        if (bb != header) {
            ret.body = bb;
        }
    }

    // the loop has too many exiting edges
    if (loop.exits.size() > 1) {
        return nullopt;
    }

    // the exiting bb is not header
    auto [exiting, exit] = *loop.exits.begin();
    if (exiting != header) {
        return nullopt;
    }

    // parse exit
    ret.exit = exit;

    // parse preheader
    ret.preheader = loop.preheader;

    // induction var
    auto br_inst = ret.header->insts().back().as<BrInst>();
    assert(br_inst->operands().size() == 3);
    auto cond = br_inst->get_operand(0);
    // the induction variable should be of int type
    if (not cond->is<ICmpInst>()) {
        return nullopt;
    }

    auto icmp_inst = cond->as<ICmpInst>();
    auto icmp_op = icmp_inst->get_icmp_op();
    auto lhs = icmp_inst->lhs();
    auto rhs = icmp_inst->rhs();

    auto exit_cond = [&](bool is_ind_rhs) {
        auto opposite = ICmpInst::opposite_icmp_op(icmp_op);
        auto op = is_ind_rhs ? opposite : icmp_op;
        if (br_inst->get_operand(1) == ret.exit) {
            // exit if cond is true
            return op;
        } else if (br_inst->get_operand(2) == ret.exit) {
            // exit if cond is false
            return ICmpInst::not_icmp_op(op);
        } else {
            throw unreachable_error{};
        }
    };

    // at least one op should be mutable after constant folding
    assert(not lhs->is<ConstInt>() or not rhs->is<ConstInt>());
    if (lhs->is<ConstInt>()) {
        ret.ind_var = rhs;
        ret.icmp_op = exit_cond(true);
        ret.bound = lhs->as<ConstInt>();
    } else if (rhs->is<ConstInt>()) {
        ret.ind_var = lhs;
        ret.icmp_op = exit_cond(false);
        ret.bound = rhs->as<ConstInt>();
    } else {
        return nullopt;
    }

    // find initial and step
    for (auto &&inst : header->insts()) {
        if (not inst.is<PhiInst>()) {
            break;
        }
        if (&inst != ret.ind_var) {
            continue;
        }
        // the phi for induction variable
        auto phi_inst = inst.as<PhiInst>();
        // the phi should has two source, one from preheader and the other from
        // loop body
        assert(phi_inst->to_pairs().size() == 2);
        for (auto [value, source] : phi_inst->to_pairs()) {
            if (contains(loop.bbs, source)) {
                // step
                if (not value->is<IBinaryInst>()) {
                    continue;
                }
                auto ibinary_inst = value->as<IBinaryInst>();
                auto ibin_op = ibinary_inst->get_ibin_op();
                if (ibin_op != IBinaryInst::ADD) {
                    continue;
                }
                if (ibinary_inst->lhs()->is<ConstInt>() and
                    ibinary_inst->rhs() == ret.ind_var) {
                    ret.step = ibinary_inst->lhs()->as<ConstInt>();
                } else if (ibinary_inst->rhs()->is<ConstInt>() and
                           ibinary_inst->lhs() == ret.ind_var) {
                    ret.step = ibinary_inst->rhs()->as<ConstInt>();
                }
            } else {
                // initial
                if (not value->is<ConstInt>()) {
                    continue;
                }
                ret.initial = value->as<ConstInt>();
            }
        }
    }
    if (ret.initial == nullptr or ret.step == nullptr) {
        return nullopt;
    }

    return ret;
}

bool LoopUnroll::should_unroll(const SimpleLoopInfo &simple_loop) {
    int initial = simple_loop.initial->val();
    int step = simple_loop.step->val();
    int bound = simple_loop.bound->val();

    int estimate = (bound - initial) / step;

    return estimate < UNROLL_MAX;
}

void LoopUnroll::unroll_simple_loop(const SimpleLoopInfo &simple_loop) {
    map<Value *, Value *> old2new, phi2dst;
    // set phi_var_map to initial value
    for (auto &&inst : simple_loop.header->insts()) {
        if (not inst.is<PhiInst>()) {
            break;
        }
        // the phi for induction variable
        auto phi_inst = inst.as<PhiInst>();
        // the phi should has two source, one from preheader and the other from
        // loop body
        assert(phi_inst->to_pairs().size() == 2);
        for (auto [value, source] : phi_inst->to_pairs()) {
            if (not contains(simple_loop.bbs, source)) {
                old2new.emplace(phi_inst, value);
            } else {
                phi2dst.emplace(value, phi_inst);
            }
        }
    }

    auto should_exit = [&](int i) {
        int bound = simple_loop.bound->val();
        switch (simple_loop.icmp_op) {
        case ICmpInst::EQ:
            return i == bound;
        case ICmpInst::NE:
            return i != bound;
        case ICmpInst::LT:
            return i < bound;
        case ICmpInst::LE:
            return i <= bound;
        case ICmpInst::GT:
            return i > bound;
        case ICmpInst::GE:
            return i >= bound;
        default:
            throw unreachable_error{};
        }
    };

    auto func = simple_loop.header->get_func();
    auto bb = func->create_bb();

    auto clone2bb = [&](BasicBlock *old_bb) {
        for (auto &inst : old_bb->insts()) {
            if (inst.is<BrInst>() or inst.is<PhiInst>()) {
                continue;
            }
            auto new_inst = bb->clone_inst(bb->insts().end(), &inst);

            new_inst->set_operand_for_each_if(
                [&](Value *op) -> pair<bool, Value *> {
                    if (contains(old2new, op))
                        return {true, old2new[op]};
                    else
                        return {false, nullptr};
                });
            old2new[&inst] = new_inst;
            if (contains(phi2dst, static_cast<Value *>(&inst))) {
                old2new[phi2dst[&inst]] = new_inst;
            }
        }
    };

    for (int i = simple_loop.initial->val(); not should_exit(i);
         i += simple_loop.step->val()) {
        clone2bb(simple_loop.header);
        clone2bb(simple_loop.body);
    }
    // run header when the cond is false
    clone2bb(simple_loop.header);

    for (auto [old_inst, new_inst] : old2new) {
        old_inst->replace_all_use_with(new_inst);
    }

    // connect exit
    simple_loop.header->erase_inst(&*simple_loop.header->insts().rbegin());
    bb->create_inst<BrInst>(simple_loop.exit);

    // connect preheader
    simple_loop.preheader->br_inst().replace_operand(simple_loop.header, bb);

    // remove old bbs
    for (auto it = func->bbs().begin(); it != func->bbs().end();) {
        if (contains(simple_loop.bbs, &*it)) {
            it = func->bbs().erase(it);
        } else {
            ++it;
        }
    }
}

void LoopUnroll::handle_func(Function *func, const FuncLoopInfo &func_loop) {
    for (auto &&header : func_loop.get_topo_order()) {
        auto &&loop = func_loop.loops.at(header);
        assert(loop.preheader != nullptr);
        auto simple_loop = parse_simple_loop(header, loop);
        if (not simple_loop.has_value()) {
            continue;
        }
        if (not should_unroll(simple_loop.value())) {
            continue;
        }
        debugs << "unrolling " + simple_loop->header->get_name() << '\n';
        unroll_simple_loop(simple_loop.value());
    }
}

bool LoopUnroll::run(PassManager *mgr) {
    auto &&loop_info = mgr->get_result<LoopFind>().loop_info;
    auto m = mgr->get_module();
    for (auto &&func : m->functions()) {
        if (func.is_external) {
            continue;
        }
        handle_func(&func, loop_info.at(&func));
    }
    return false;
}
