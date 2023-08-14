#include "array_visit.hh"
#include "err.hh"
#include "func_info.hh"
#include "function.hh"
#include "global_variable.hh"
#include "instruction.hh"
#include "type.hh"
#include "utils.hh"
#include <cassert>
#include <iostream>

using namespace std;
using namespace pass;
using namespace ir;

ArrayVisit::AliasResult ArrayVisit::is_alias(MemAddress *lhs, MemAddress *rhs) {
    auto l_base = lhs->get_base();
    auto r_base = rhs->get_base();
    if (lhs->get_ptr() == rhs->get_ptr())
        return AliasResult::MustAlias;
    if (is_a<GlobalVariable>(l_base)) {
        if (is_a<GlobalVariable>(r_base)) {
            if (l_base != r_base)
                return AliasResult::NoAlias;
            if (not(lhs->get_offset().has_value() &&
                    rhs->get_offset().has_value()))
                return AliasResult::MayAlias;
            if (lhs->get_offset().value() == rhs->get_offset().value())
                return AliasResult::MustAlias;
            else
                return AliasResult::NoAlias;
        } else if (is_a<Argument>(r_base))
            // r_base may be l_base as parameter passed by caller
            // func(l_base[1][2])->func(r_base)
            return AliasResult::MayAlias;
        else
            return AliasResult::NoAlias;
    } else if (is_a<AllocaInst>(l_base)) {
        if (is_a<AllocaInst>(r_base)) {
            if (l_base != r_base)
                return AliasResult::NoAlias;
            if (not(lhs->get_offset().has_value() &&
                    rhs->get_offset().has_value()))
                return AliasResult::MayAlias;
            if (lhs->get_offset().value() == rhs->get_offset().value())
                return AliasResult::MustAlias;
            else
                return AliasResult::NoAlias;
        }
        // if any of base is not AllocaInst, it is absolutely not alias
        // because AllocaInst must be local
        return AliasResult::NoAlias;
    } else if (is_a<Argument>(l_base)) {
        if (is_a<AllocaInst>(r_base))
            return AliasResult::NoAlias;
        else
            return AliasResult::MayAlias;
    }
    throw unreachable_error{};
}

void ArrayVisit::clear() {
    ptr2addr.clear();
    addrs.clear();
    latest_val.clear();
    del_store_load.clear();
}

bool ArrayVisit::run(pass::PassManager *mgr) {
    auto m = mgr->get_module();
    _func_info = &mgr->get_result<FuncInfo>();
    clear();
    for (auto &f_r : m->functions()) {
        if (f_r.is_external)
            continue;
        for (auto &bb_r : f_r.bbs()) {
            del_store_load.clear();
            latest_val.clear();
            // mem visit don't scan vals on the same MemAddress between
            // different bbs
            mem_visit(&bb_r);
            for (auto store : del_store_load) {
                bb_r.erase_inst(store);
            }
        }
    }
    return false;
}

void ArrayVisit::mem_visit(BasicBlock *bb) {
    for (auto &inst_r : bb->insts()) {
        auto inst = &inst_r;
        if (is_a<StoreInst>(inst)) {
            // if store is an array initializer, continue!
            if (inst->get_operand(0)->get_type()->is<ArrayType>())
                continue;
            auto ptr = inst->get_operand(1);
            auto mem = alias_analysis(ptr);
            if (latest_val[mem]) {
                if (inst->get_operand(0) == latest_val[mem]) {
                    // if the same val is stored twice, then delete the second
                    // store
                    del_store_load.insert(inst);
                } else {
                    latest_val[mem] = inst->get_operand(0);
                }
            } else {
                latest_val[mem] = inst->get_operand(0);
            }
        }
        // if mem based on the inst has the latest val meaning that this load
        // can be replaced with val
        else if (is_a<LoadInst>(inst)) {
            auto ptr = inst->get_operand(0);
            if (ptr2addr[ptr]) {
                if (latest_val[ptr2addr[ptr]]) {
                    inst->replace_all_use_with(latest_val[ptr2addr[ptr]]);
                    del_store_load.insert(inst);
                }
            }
        }
        // take a conservative strategy that any non_pure function may change
        // the current latest vals
        else if (is_a<CallInst>(inst) &&
                 not _func_info->is_pure_function(
                     as_a<Function>(inst->get_operand(0)))) {
            latest_val.clear();
        }
    }
}

// create MemAddress based on ptr and scan whether there is a alias MemAddress
ArrayVisit::MemAddress *ArrayVisit::alias_analysis(Value *ptr) {
    auto new_mem = MemAddress(ptr);
    MemAddress *ret_mem = nullptr;
    for (auto mem : addrs) {
        auto res = is_alias(mem, &new_mem);
        // MustAlias: return MemAddress
        // MayAlias: clear the val on the MemAddress
        // NoAlias: do nothing
        switch (res) {
        case AliasResult::MustAlias:
            assert(ret_mem == nullptr);
            ret_mem = mem;
            break;
        case AliasResult::MayAlias:
            latest_val[mem] = nullptr;
            break;
        case AliasResult::NoAlias:
            break;
        }
    }
    if (not ret_mem) {
        ret_mem = new MemAddress(new_mem);
        addrs.insert(ret_mem);
    }
    return ptr2addr[ptr] = ret_mem;
}
