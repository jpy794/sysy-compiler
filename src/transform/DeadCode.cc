#include "DeadCode.hh"
#include "func_info.hh"
#include "function.hh"
#include "global_variable.hh"
#include "instruction.hh"
#include "module.hh"
#include "utils.hh"

#include <vector>

using namespace std;
using namespace pass;
using namespace ir;

void DeadCode::run(PassManager *mgr) {
    _func_info = &mgr->get_result<FuncInfo>();
    auto m = mgr->get_module();
    for (auto &f_r : m->functions()) {
        auto f = &f_r;
        mark_sweep(f);
    }
    sweep_globally(m);
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
                continue;
            auto op_inst = as_a<Instruction>(op);
            marked[op_inst] = true;
            work_list.push_back(op_inst);
        }
        // TODO: elimate useless control flow
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
            iter->replace_all_use_with(nullptr);
            iter = bb.erase_inst(&*iter);
        }
    }
}

bool DeadCode::is_critical(Instruction *inst) {
    if (is_a<RetInst>(inst) || is_a<StoreInst>(inst) || is_a<BrInst>(inst))
        return true;
    if (is_a<CallInst>(inst) &&
        not _func_info->is_pure_function(as_a<Function>(inst->operands()[0])))
        return true;
    return false;
}

void DeadCode::sweep_globally(Module *m) {
    vector<Function *> unused_funcs;
    vector<GlobalVariable *> unused_globals;
    for (auto &f_r : m->functions()) {
        if (f_r.get_use_list().size() == 0 and &f_r != m->get_main())
            unused_funcs.push_back(&f_r);
    }
    for (auto &glob_var_r : m->global_vars()) {
        if (glob_var_r.get_use_list().size() == 0)
            unused_globals.push_back(&glob_var_r);
    }
    for (auto func : unused_funcs)
        m->functions().erase(func);
    for (auto glob : unused_globals)
        m->global_vars().erase(glob);
}
