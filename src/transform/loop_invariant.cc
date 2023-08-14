#include "loop_invariant.hh"
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

bool LoopInvariant::is_side_effect_inst(Instruction *inst) {
    return inst->is<LoadInst>() || inst->is<StoreInst>() ||
           inst->is<CallInst>() || inst->is<RetInst>() || inst->is<BrInst>() ||
           inst->is<PhiInst>();
}

vector<Instruction *>
LoopInvariant::collect_invariant_inst(BasicBlock *bb, const LoopInfo &loop) {
    vector<Instruction *> ret;
    for (auto &&inst : bb->insts()) {
        if (is_side_effect_inst(&inst)) {
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

void LoopInvariant::run(PassManager *mgr) {
    auto &&loop_info = mgr->get_result<LoopFind>().loop_info;
    auto m = mgr->get_module();
    for (auto &&func : m->functions()) {
        if (func.is_external) {
            continue;
        }
        handle_func(&func, loop_info.at(&func));
    }
}
