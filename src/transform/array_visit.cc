#include "array_visit.hh"
#include "basic_block.hh"
#include "depth_order.hh"
#include "err.hh"
#include "func_info.hh"
#include "function.hh"
#include "global_variable.hh"
#include "instruction.hh"
#include "type.hh"
#include "utils.hh"
#include "value.hh"
#include <algorithm>
#include <cassert>

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
    addrs.clear();
    latest_val.clear();
    del_store_load.clear();
    visited.clear();
}

bool ArrayVisit::run(pass::PassManager *mgr) {
    auto m = mgr->get_module();
    _func_info = &mgr->get_result<FuncInfo>();
    _depth_order = &mgr->get_result<DepthOrder>();
    clear();
    bool ir_changed = false;
    for (auto &f_r : m->functions()) {
        if (f_r.is_external)
            continue;
        bool mem_changed = true;
        while (mem_changed) {
            mem_changed = false;
            visited.clear();
            latest_val.clear();
            // analysis
            bool iter = true;
            while (iter) {
                iter = false;
                replace_table.clear();
                del_store_load.clear();
                for (auto bb_p : _depth_order->_depth_priority_order.at(&f_r)) {
                    bb = bb_p;
                    auto prev_latest_vals = latest_val[bb];
                    latest_val[bb] = join(bb);
                    visited[bb] = true;
                    mem_visit(bb);
                    if (not equal(prev_latest_vals, latest_val[bb]))
                        iter = true;
                }
            }
            // change ir
            if (replace_table.size() || del_store_load.size()) {
                ir_changed = true;
                mem_changed = true;
            }
            for (auto [inst, val] : replace_table) {
                while (is_a<Instruction>(val) &&
                       contains(replace_table, as_a<Instruction>(val))) {
                    val = replace_table[as_a<Instruction>(val)];
                }
                inst->replace_all_use_with(val);
            }
            for (auto inst : del_store_load) {
                inst->get_parent()->erase_inst(inst);
            }
            // delete MemAddress
            for (auto mem : addrs) {
                delete mem;
            }
            addrs.clear();
        }
    }
    return ir_changed;
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
            if (latest_val[bb][mem]) {
                if (inst->get_operand(0) == latest_val[bb][mem]) {
                    // if the same val is stored twice, then delete the
                    // second store
                    del_store_load.insert(inst);
                } else {
                    latest_val[bb][mem] = inst->get_operand(0);
                }
            } else {
                latest_val[bb][mem] = inst->get_operand(0);
            }
        } else if (is_a<LoadInst>(inst)) {
            auto ptr = inst->get_operand(0);
            MemAddress *mem = alias_analysis(ptr, false);
            // if mem has the latest val, then replace it
            if (latest_val[bb][mem]) {
                replace_table[inst] = latest_val[bb][mem];
                del_store_load.insert(inst);
            }
            // the load inst is the latest val
            else {
                latest_val[bb][mem] = inst;
            }
        }
        // take a conservative strategy that any non_pure function may
        // changed the current latest vals
        else if (is_a<CallInst>(inst) &&
                 not _func_info->is_pure_function(
                     as_a<Function>(inst->get_operand(0)))) {
            latest_val[bb].clear();
        }
    }
}

// create MemAddress based on ptr and scan whether there is a alias MemAddress
ArrayVisit::MemAddress *ArrayVisit::alias_analysis(Value *ptr, bool clear) {
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
            if (clear)
                latest_val[bb][mem] = nullptr;
            break;
        case AliasResult::NoAlias:
            break;
        }
    }
    if (not ret_mem) {
        ret_mem = new MemAddress(new_mem);
        addrs.insert(ret_mem);
    }
    return ret_mem;
}

map<ArrayVisit::MemAddress *, Value *> ArrayVisit::join(BasicBlock *bb) {
    set<BasicBlock *>::iterator iter;
    map<ArrayVisit::MemAddress *, Value *> in_latest_val{};
    for (iter = bb->pre_bbs().begin(); iter != bb->pre_bbs().end(); iter++) {
        if (visited[*iter]) {
            in_latest_val = latest_val[*iter++];
            break;
        }
    }
    for (; iter != bb->pre_bbs().end(); iter++) {
        if (visited[*iter]) {
            auto tmp = latest_val[*iter];
            for (auto [mem, val] : in_latest_val) {
                if (contains(latest_val[*iter], mem)) {
                    if (in_latest_val[mem] != latest_val[*iter][mem]) {
                        in_latest_val[mem] = nullptr;
                    }
                } else {
                    in_latest_val[mem] = nullptr;
                }
            }
        }
    }
    return in_latest_val;
}

bool ArrayVisit::equal(map<MemAddress *, Value *> &mem_vals1,
                       map<MemAddress *, Value *> &mem_vals2) {
    if (mem_vals1.size() != mem_vals2.size())
        return false;
    auto l_iter = mem_vals1.begin();
    auto r_iter = mem_vals2.begin();
    for (; l_iter != mem_vals1.end(); l_iter++, r_iter++) {
        if (l_iter->first != r_iter->first)
            return false;
        else if (l_iter->second != r_iter->second)
            return false;
    }
    return true;
}