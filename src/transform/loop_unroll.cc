#include "loop_unroll.hh"
#include "basic_block.hh"
#include "constant.hh"
#include "err.hh"
#include "instruction.hh"
#include "log.hh"
#include "pass.hh"
#include "usedef_chain.hh"
#include "utils.hh"

using namespace pass;
using namespace ir;
using namespace std;

optional<LoopUnroll::SimpleLoopInfo>
LoopUnroll::parse_simple_loop(BasicBlock *header, const LoopInfo &loop) {
    SimpleLoopInfo ret;

    ret.bbs = loop.bbs;

    // loop bbs
    ret.header = header;
    // the loop has too many bbs
    if (loop.bbs.size() > 2) {
        return nullopt;
    }
    for (auto &&bb : loop.bbs) {
        if (bb != header) {
            ret.body = bb;
        }
    }
    // the loop has too many exiting edges
    if (ret.body->suc_bbs().size() > 1) {
        return nullopt;
    }

    for (auto &suc : header->suc_bbs()) {
        if (not contains(loop.bbs, suc)) {
            ret.exit = suc;
        }
    }
    assert(header->suc_bbs().size() == 2);
    assert(ret.exit != nullptr);

    // the exit is simple so that we do not need to handle phi in exit
    assert(ret.exit->pre_bbs().size() == 1);

    for (auto &pre : header->pre_bbs()) {
        if (not contains(loop.bbs, pre)) {
            ret.preheader = pre;
        }
    }
    assert(header->pre_bbs().size() == 2);
    assert(ret.preheader != nullptr);

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

void LoopUnroll::unroll_simple_loop(const SimpleLoopInfo &simple_loop,
                                    const UseDefRes &use_def) {
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
            for (auto &op : new_inst->operands()) {
                if (contains(old2new, op)) {
                    op = old2new[op];
                }
            }
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
        use_def.replace_all_use_with(old_inst, new_inst);
    }

    // connect exit
    simple_loop.header->erase_inst(--simple_loop.header->insts().end());
    bb->create_inst<BrInst>(simple_loop.exit);

    // connect preheader
    simple_loop.preheader->erase_inst(--simple_loop.preheader->insts().end());
    simple_loop.preheader->create_inst<BrInst>(bb);

    // remove old bbs
    for (auto it = func->bbs().begin(); it != func->bbs().end();) {
        if (contains(simple_loop.bbs, &*it)) {
            it = func->bbs().erase(it);
        } else {
            ++it;
        }
    }
}

void LoopUnroll::handle_func(Function *func, const FuncLoopInfo &loops,
                             const UseDefRes &use_def) {
    for (auto &&[header, loop] : loops) {
        auto simple_loop = parse_simple_loop(header, loop);
        if (not simple_loop.has_value()) {
            continue;
        }
        if (not should_unroll(simple_loop.value())) {
            continue;
        }
        debugs << "unrolling " + simple_loop->header->get_name() << '\n';
        unroll_simple_loop(simple_loop.value(), use_def);
    }
}

void LoopUnroll::run(pass::PassManager *mgr) {
    auto &&loop_info = mgr->get_result<LoopFind>().loop_info;
    auto &&use_def = mgr->get_result<UseDefChain>();
    auto m = mgr->get_module();
    for (auto &&func : m->functions()) {
        if (func.is_external) {
            continue;
        }
        handle_func(&func, loop_info.at(&func), use_def);
    }
}
