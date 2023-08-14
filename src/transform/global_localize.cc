#include "global_localize.hh"
#include "basic_block.hh"
#include "constant.hh"
#include "err.hh"
#include "function.hh"
#include "global_variable.hh"
#include "instruction.hh"
#include "type.hh"
#include <cassert>
#include <vector>

using namespace std;
using namespace ir;
using namespace pass;

bool GlobalVarLocalize::NeedMem2reg;
bool GlobalVarLocalize::NeedConstPro;

ir::Type *GlobalVarLocalize::global_type(ir::GlobalVariable *global_var) {
    return global_var->get_type()->as<PointerType>()->get_elem_type();
}

GlobalVarLocalize::Action GlobalVarLocalize::parse(GlobalVariable *global_var,
                                                   Function *main) {
    if (global_var->get_use_list().size() == 0)
        return JustSkip;
    auto type = global_type(global_var);
    if (type->is_basic_type()) { // sink only for main use
        for (auto &[user, _] : global_var->get_use_list()) {
            if (as_a<Instruction>(user)->get_parent()->get_func() != main)
                return JustSkip;
        }
        return BaseTypeSink;
    } else if (type->is<ArrayType>()) { // prop const for case with no store
        bool no_write = true;
        for (auto &[user, _] : global_var->get_use_list()) {
            auto gep = as_a<GetElementPtrInst>(user);
            for (auto &[gep_user, _] : gep->get_use_list()) {
                if (not is_a<LoadInst>(gep_user)) {
                    no_write = false;
                    break;
                }
            }
        }
        return no_write ? ConstArrProp : JustSkip;
    } else
        throw unreachable_error{};
}

bool GlobalVarLocalize::run(PassManager *mgr) {
    bool run_sink = false, run_prop = false;
    for (auto &global_var : mgr->get_module()->global_vars()) {
        switch (parse(&global_var, mgr->get_module()->get_main())) {
        case BaseTypeSink:
            run_sink = true;
            sink(&global_var);
            break;
        case ConstArrProp:
            run_prop = true;
            prop_const(&global_var);
            break;
        case JustSkip:
            continue;
        }
    }
    NeedMem2reg = run_sink;
    NeedConstPro = run_prop;
    return NeedMem2reg or NeedConstPro;
}

void GlobalVarLocalize::sink(GlobalVariable *global_var) {
    auto func = as_a<Instruction>(global_var->get_use_list().begin()->user)
                    ->get_parent()
                    ->get_func();
    auto entry = func->get_entry_bb();
    auto iter = entry->insts().begin();
    auto alloc = entry->insert_inst<AllocaInst>(iter, global_type(global_var));
    for (; iter != entry->insts().end(); ++iter)
        if (not is_a<AllocaInst>(&*iter))
            break;
    entry->insert_inst<StoreInst>(iter, global_var->get_init(), alloc);
    global_var->replace_all_use_with(alloc);
}

void GlobalVarLocalize::prop_const(GlobalVariable *global_var) {
    auto arr_type = global_type(global_var)->as<ArrayType>();
    for (auto &[user, _] : global_var->get_use_list()) {
        auto gep = as_a<GetElementPtrInst>(user);
        vector<int> idxs;
        for (auto idx : gep->operands()) {
            if (idx == global_var)
                continue;
            if (not is_a<ConstInt>(idx))
                break;
            idxs.push_back(as_a<ConstInt>(idx)->val());
        }
        if (idxs.size() + 1 != gep->operands().size())
            continue;
        assert(idxs.size() == 1 + arr_type->get_dims());
        assert(idxs[0] == 0);
        // find corresponding init value
        bool zero_init = false;
        auto init = global_var->get_init();
        for (unsigned i = 1; i < idxs.size(); ++i) {
            if (is_a<ConstZero>(init)) {
                zero_init = true;
                break;
            } else if (is_a<ConstArray>(init)) {
                init = as_a<ConstArray>(init)->array().at(idxs[i]);
            } else
                throw unreachable_error{};
        }
        Constant *const_v{nullptr};
        if (zero_init)
            const_v = Constants::get().zero_const(arr_type->get_base_type());
        else
            const_v = init;
        // replace load use with const init value
        for (auto &[load_user, _] : gep->get_use_list())
            load_user->replace_all_use_with(const_v);
    }
}
