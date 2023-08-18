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

    // parse header
    ret.header = header;
    ret.bbs = loop.bbs;

    // parse body
    for (auto &&bb : loop.bbs) {
        if (bb != header) {
            ret.bodies.insert(bb);
        }
    }

    if (loop.sub_loops.size() > 0) {
        return nullopt;
    }

    if (loop.latches.size() > 1) {
        return nullopt;
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
    auto header = simple_loop.header;

    // topological sort
    vector<BasicBlock *> bodies_order;
    map<BasicBlock *, size_t> pre_cnt;
    for (auto bb : simple_loop.bodies) {
        if (contains(bb->pre_bbs(), header)) {
            pre_cnt[bb] = 0;
        } else {
            pre_cnt[bb] = bb->pre_bbs().size();
        }
    }
    while (not pre_cnt.empty()) {
        bool changed{false};
        for (auto [bb, cnt] : pre_cnt) {
            if (cnt == 0) {
                bodies_order.push_back(bb);
                for (auto suc : bb->suc_bbs()) {
                    if (suc != header) {
                        pre_cnt[suc]--;
                    }
                }
                pre_cnt.erase(bb);
                changed = true;
                break;
            }
        }
        assert(changed);
    }

    assert(bodies_order.size() == simple_loop.bodies.size());

    map<Value *, Value *> old2new, phi_dst2src;
    // set phi_var_map to initial value
    for (auto &&inst : header->insts()) {
        if (not inst.is<PhiInst>()) {
            break;
        }
        // the phi for induction variable
        auto phi_inst = inst.as<PhiInst>();
        // the phi should has two source, one from preheader and the other from
        // loop body
        assert(phi_inst->to_pairs().size() == 2);
        for (auto [value, source] : phi_inst->to_pairs()) {
            if (contains(simple_loop.bbs, source)) {
                phi_dst2src.emplace(phi_inst, value);
            }
        }
        for (auto [value, source] : phi_inst->to_pairs()) {
            if (not contains(simple_loop.bbs, source)) {
                old2new.emplace(phi_dst2src[phi_inst], value);
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

    auto func = header->get_func();

    auto clone_bbs = [&]() {
        for (auto bb : bodies_order) {
            old2new[bb] = func->create_bb();
        }
        old2new[header] = func->create_bb();
    };

    auto clone2bb = [&](BasicBlock *old_bb) {
        auto new_bb = old2new.at(old_bb)->as<BasicBlock>();
        for (auto &inst : old_bb->insts()) {
            if (old_bb == header) {
                if (inst.is<PhiInst>()) {
                    old2new[&inst] = old2new[phi_dst2src[&inst]];
                    continue;
                } else if (inst.is<BrInst>()) {
                    continue;
                }
            }
            auto new_inst = new_bb->clone_inst(new_bb->insts().end(), &inst);

            new_inst->set_operand_for_each_if(
                [&](Value *op) -> pair<bool, Value *> {
                    if (contains(old2new, op))
                        return {true, old2new[op]};
                    else
                        return {false, nullptr};
                });
            old2new[&inst] = new_inst;
        }
    };

    auto unroll_exit = [&]() { return old2new[header]->as<BasicBlock>(); };
    auto unroll_entry = [&]() {
        if (contains(old2new, static_cast<Value *>(bodies_order.front()))) {
            return old2new[bodies_order.front()]->as<BasicBlock>();
        } else {
            return old2new[header]->as<BasicBlock>();
        }
    };

    old2new[header] = func->create_bb();
    clone2bb(header);
    auto bbs_entry = unroll_entry();

    for (int i = simple_loop.initial->val(); not should_exit(i);
         i += simple_loop.step->val()) {
        // connect last exit to current entry
        auto last_exit = unroll_exit();
        clone_bbs();
        auto entry = unroll_entry();
        last_exit->create_inst<BrInst>(entry);

        // clone bbs and header
        for (auto bb : bodies_order) {
            clone2bb(bb);
        }
        clone2bb(header);
    }

    for (auto [old_inst, new_inst] : old2new) {
        if (not old_inst->is<Instruction>()) {
            continue;
        }
        old_inst->replace_all_use_with(new_inst);
    }

    // connect exit
    header->erase_inst(&header->br_inst());
    unroll_exit()->create_inst<BrInst>(simple_loop.exit);

    // connect preheader
    simple_loop.preheader->br_inst().replace_operand(header, bbs_entry);

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
