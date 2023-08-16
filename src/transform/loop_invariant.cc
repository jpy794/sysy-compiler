#include "loop_invariant.hh"
#include "dominator.hh"
#include "log.hh"

using namespace pass;
using namespace ir;
using namespace std;

bool LoopInvariant::is_invariant_operand(Value *op, const LoopInfo &loop) {
    if (not op->is<Instruction>()) {
        return true;
    }
    auto inst = as_a<Instruction>(op);
    return not contains(loop.bbs, inst->get_parent());
}

bool LoopInvariant::is_dom_store(Instruction *inst, const LoopInfo &loop) {
    if (not inst->is<StoreInst>()) {
        return false;
    }

    auto get_gep_base = [](Instruction *inst) {
        Value *ptr{nullptr};
        if (inst->is<LoadInst>()) {
            ptr = inst->as<LoadInst>()->ptr();
        } else if (inst->is<StoreInst>()) {
            ptr = inst->as<StoreInst>()->ptr();
        } else {
            throw unreachable_error{};
        }
        if (not ptr->is<GetElementPtrInst>()) {
            return ptr;
        }
        auto gep = ptr->as<GetElementPtrInst>();
        while (true) {
            if (not gep->base_ptr()->is<GetElementPtrInst>()) {
                return gep->base_ptr();
            }
            gep = gep->base_ptr()->as<GetElementPtrInst>();
        }
        throw unreachable_error{};
    };

    auto may_alias = [&](Instruction *other) {
        if (not(other->is<LoadInst>() or other->is<StoreInst>())) {
            return false;
        }
        auto inst_base = get_gep_base(inst);
        auto other_base = get_gep_base(other);
        if (inst_base->is<AllocaInst>() and other_base->is<AllocaInst>()) {
            return inst_base == other_base;
        }
        return true;
    };

    bool dom_in_bb{true};
    for (auto &&other : inst->get_parent()->insts()) {
        if (&other == inst) {
            break;
        }
        if (may_alias(&other)) {
            dom_in_bb = false;
            break;
        }
    }
    bool dom_out_bb{true};
    auto cur_bb = inst->get_parent();
    for (auto &&bb : loop.bbs) {
        if (_dom->is_dom(cur_bb, bb)) {
            continue;
        }
        for (auto &&other : bb->insts()) {
            if (may_alias(&other)) {
                dom_out_bb = false;
                break;
            }
        }
    }
    return dom_in_bb and dom_out_bb;
}

bool LoopInvariant::is_side_effect_inst(Instruction *inst) {
    return inst->is<LoadInst>() || inst->is<StoreInst>() ||
           inst->is<CallInst>() || inst->is<RetInst>() || inst->is<BrInst>() ||
           inst->is<PhiInst>();
}

vector<Instruction *>
LoopInvariant::collect_invariant_inst(BasicBlock *bb, const LoopInfo &loop) {
    vector<Instruction *> ret;
    for (auto &&inst : bb->insts()) {
        if (is_side_effect_inst(&inst) and not is_dom_store(&inst, loop)) {
            continue;
        }
        bool invariant{true};
        for (auto &&op : inst.operands()) {
            if (not is_invariant_operand(op, loop)) {
                invariant = false;
                break;
            }
        }
        if (invariant) {
            ret.push_back(&inst);
        }
    }
    return ret;
}

void LoopInvariant::handle_func(Function *func, const FuncLoopInfo &func_loop) {
    for (auto &&header : func_loop.get_topo_order()) {
        auto &&loop = func_loop.loops.at(header);
        assert(loop.preheader != nullptr);
        auto preheader = loop.preheader;
        bool changed{true};
        while (changed) {
            vector<Instruction *> insts;
            for (auto bb : loop.bbs) {
                auto bb_insts = collect_invariant_inst(bb, loop);
                insts.insert(insts.end(), bb_insts.begin(), bb_insts.end());
            }
            changed = insts.size() > 0;
            for (auto inst : insts) {
                preheader->move_inst(&*preheader->insts().rbegin(), inst);
            }
        }

        /* debugs << "invariant of loop " << header->get_name();
         * for (auto &&inst : preheader->insts()) {
         *     debugs << ' ' << inst.get_name();
         * }
         * debugs << '\n'; */
    }
}

bool LoopInvariant::run(PassManager *mgr) {
    auto &&loop_info = mgr->get_result<LoopFind>().loop_info;
    _dom = &mgr->get_result<Dominator>();
    auto m = mgr->get_module();
    for (auto &&func : m->functions()) {
        if (func.is_external) {
            continue;
        }
        handle_func(&func, loop_info.at(&func));
    }
    return false;
}
