#include "DeadCode.hh"
#include "func_info.hh"
#include "function.hh"
#include "instruction.hh"
#include "utils.hh"

using namespace pass;
using namespace ir;

void DeadCode::run(PassManager *mgr) {
    _func_info = &mgr->get_result<FuncInfo>();
    auto m = mgr->get_module();
    for (auto &f_r : m->functions()) {
        auto f = &f_r;
        mark_sweep(f);
    }
}

void DeadCode::mark_sweep(Function *func) {
    work_list.clear();
    marked.clear();
    // Initialize the work list with critical instuction
    for (auto &bb : func->bbs()) {
        for (auto &inst_r : bb.insts()) {
            auto inst = &inst_r;
            if (is_critical(inst)) {
                marked[inst] = true;
                work_list.push_back(inst);
            }
        }
    }
    // Perform the Mark phase
    mark();
    // Perfrom the Sweep phase
    sweep(func);
}

// Perform the Mark phase
void DeadCode::mark() {
    while (not work_list.empty()) {
        auto inst = work_list.front();
        work_list.pop_front();
        for (unsigned i = 0; i < inst->operands().size(); i++) {
            auto op = inst->get_operand(i);
            if (not is_a<Instruction>(op) || marked[as_a<Instruction>(op)])
                continue; // FIXME:For bb, it's requirable to be
                          // processed
            auto op_inst = as_a<Instruction>(op);
            marked[op_inst] = true;
            work_list.push_back(op_inst);
        }
        /*TODO:
        for each b ∈ RDF(block(i))
            mark the block-ending branch in b
            add it to WorkList
        */
    }
}

// Perfrom the Sweep phase
void DeadCode::sweep(Function *func) {
    for (auto &bb : func->bbs()) {
        auto &insts = bb.insts();
        for (auto iter = insts.begin(); iter != insts.end();) {
            if (marked[&*iter]) {
                ++iter;
                continue;
            }
            // FIXME: inst may be a branch
            iter = insts.erase(iter);
        }
    }
}

bool DeadCode::is_critical(Instruction *inst) {
    if (is_a<RetInst>(inst) || is_a<StoreInst>(inst) || is_a<BrInst>(inst))
        return true;
    if (is_a<CallInst>(inst) &&
        not _func_info->is_pure_function(as_a<Function>(inst->operands()[0])))
        return true;
    // if (is_a<CallInst>(inst))
    //     return true;
    return false;
}