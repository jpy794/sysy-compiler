#include "func_info.hh"
#include "function.hh"
#include "global_variable.hh"
#include "instruction.hh"
#include "type.hh"
#include "utils.hh"
#include <cassert>
#include <stdexcept>

using namespace pass;
using namespace ir;
using namespace std;

void FuncInfo::run(PassManager *mgr) {
    clear();
    auto m = mgr->get_module();
    // calculate the pure function
    for (auto &f_r : m->functions()) {
        // init
        if (_result.callers.find(&f_r) == _result.callers.end())
            _result.callers[&f_r] = {};
        for (auto &bb_r : f_r.bbs()) {
            for (auto &inst_r : bb_r.insts()) {
                auto inst = &inst_r;
                if (is_a<CallInst>(inst)) {
                    _result.callers[as_a<Function>(inst->operands()[0])].insert(
                        &f_r);
                }
            }
        }
    }
    // calculate the probable pure func
    for (auto &f_r : m->functions()) {
        auto f = &f_r;
        if (maybe_pure(f)) {
            _result.pure_functions.insert(f);
        }
    }
    // iterate to delete functions that call impure funcion
    while (not callee_pure.empty()) {
        auto callee = callee_pure.front();
        callee_pure.pop_front();
        if (_result.is_pure_function(callee))
            continue;
        for (auto caller : _result.callers.at(callee)) {
            if (_result.is_pure_function(caller)) {
                _result.pure_functions.erase(caller);
                callee_pure.push_back(caller);
            }
        }
    }
}

bool FuncInfo::maybe_pure(Function *func) {
    if (func->is_external or func->get_name() == "main")
        return false;
    for (auto &bb : func->bbs()) {
        for (auto &inst : bb.insts()) {
            if (is_side_effect_inst(&inst)) {
                return false;
            }
        }
    }
    return true;
}

// This can only focus on part around it the inst may not be a side-effect inst
// before mem2reg & DCE
bool FuncInfo::is_side_effect_inst(Instruction *inst) {
    Value *addr = nullptr;
    if (is_a<LoadInst>(inst)) { // it can lead to a different return value for
                                // the same params
        addr = get_origin_addr(inst->get_operand(0));
    } else if (is_a<StoreInst>(inst)) { // it can change non-local
                                        // variables(globalvar,argument)
        addr = get_origin_addr(inst->get_operand(1));
    } else if (is_a<CallInst>(
                   inst)) { // it can result the same thing as the two above
        // deal with it after excluding most impure function
        auto callee = as_a<Function>(inst->operands()[0]);
        if (not contains(callee_pure, callee))
            callee_pure.push_back(callee);
        return false;
    } else {
        return false;
    }
    if (is_a<AllocaInst>(addr))
        return false;
    else
        return true;
}

// address may be local, global, argument
Value *FuncInfo::get_origin_addr(Value *addr) {
    if (is_a<GetElementPtrInst>(addr))
        return get_origin_addr(as_a<Instruction>(addr)->get_operand(0));
    else if (is_a<AllocaInst>(addr) || is_a<Argument>(addr) ||
             is_a<GlobalVariable>(addr))
        return addr;
    throw logic_error{addr->get_name() + "is not a address"};
}