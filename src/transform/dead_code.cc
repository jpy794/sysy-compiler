#include "dead_code.hh"
#include "err.hh"
#include "func_info.hh"
#include "function.hh"
#include "global_variable.hh"
#include "instruction.hh"
#include "log.hh"
#include "module.hh"
#include "type.hh"
#include "utils.hh"

#include <algorithm>

using namespace std;
using namespace pass;
using namespace ir;

bool DeadCode::run(PassManager *mgr) {
    _func_info = &mgr->get_result<FuncInfo>();
    auto m = mgr->get_module();
    changed = false;
    for (auto &f_r : m->functions()) {
        if (f_r.is_external)
            continue;
        auto f = &f_r;
        mark_sweep(f);
    }
    sweep_globally(m);
    return changed;
}

void DeadCode::mark_sweep(Function *func) {
    work_list.clear();
    marked.clear();
    store_not_critical.clear();
    // prepare for is_critical
    collect_store_not_critical(func);
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
            changed = true;
        }
    }
}

bool DeadCode::is_critical(Instruction *inst) {
    if (is_a<RetInst>(inst) || is_a<BrInst>(inst))
        return true;
    if (is_a<StoreInst>(inst))
        return not contains(store_not_critical, inst);
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
    changed |= unused_funcs.size() or unused_globals.size();
    for (auto func : unused_funcs)
        m->functions().erase(func);
    for (auto glob : unused_globals)
        m->global_vars().erase(glob);
}

void DeadCode::collect_store_not_critical(Function *func) {
    for (auto &bb : func->bbs()) {
        for (auto &alloca : bb.insts()) {
            if (not is_a<AllocaInst>(&alloca))
                continue;
            if (alloca.get_type()
                    ->as<PointerType>()
                    ->get_elem_type()
                    ->is_basic_type())
                continue;
            bool alloca_is_critical = false;
            unordered_set<Instruction *> related_store;
            for (auto &[arr_use, _] : alloca.get_use_list()) {
                // store [value], arr-ptr
                if (is_a<StoreInst>(arr_use)) {
                    related_store.insert(as_a<Instruction>(arr_use));
                    continue;
                }
                if (is_a<Ptr2IntInst>(arr_use)) {
                    // algebraication
                    // TODO
                    alloca_is_critical = true;
                } else if (is_a<GetElementPtrInst>(arr_use)) {
                    // gep arr-ptr, [offs]
                    for (auto &[use, _] :
                         as_a<GetElementPtrInst>(arr_use)->get_use_list()) {
                        if (is_a<StoreInst>(use)) {
                            related_store.insert(as_a<Instruction>(use));
                        } else {
                            alloca_is_critical = true;
                            break;
                        }
                    }
                }
                if (alloca_is_critical)
                    break;
            }
            if (not alloca_is_critical) {
                auto old_set = store_not_critical;
                set_union(
                    related_store.begin(), related_store.end(), old_set.begin(),
                    old_set.end(),
                    inserter(store_not_critical, store_not_critical.begin()));
            }
        }
    }
}
