#include "naive_rec_opt.hh"
#include "constant.hh"
#include "function.hh"
#include "instruction.hh"
#include "utils.hh"

using namespace std;
using namespace pass;
using namespace ir;

bool NaiveRecOpt::run(PassManager *mgr) {
    auto m = mgr->get_module();
    for (auto &&func : m->functions()) {
        if (func.is_external) {
            continue;
        }
        handle_func(&func);
    }
    return false;
}

bool is_naive_rec(Function *func) {
    if (not func->is_recursion()) {
        return false;
    }
    vector<CallInst *> rec_calls;
    for (auto &&call : func->get_use_list()) {
        if (call.user->as<Instruction>()->get_parent()->get_func() == func) {
            rec_calls.push_back(call.user->as<CallInst>());
        }
    }
    if (rec_calls.size() != 2) {
        return false;
    }
    auto is_naive_rec_call = [&](CallInst *a) {
        if (a->get_operand(2)->is<IBinaryInst>()) {
            auto ibin = a->get_operand(2)->as<IBinaryInst>();
            if (ibin->get_ibin_op() == ir::IBinaryInst::SUB and
                ibin->lhs() == func->get_args()[1]) {
                return true;
            }
        }
        return false;
    };
    return is_naive_rec_call(rec_calls[0]) and is_naive_rec_call(rec_calls[1]);
}

void NaiveRecOpt::handle_func(Function *func) {
    if (is_naive_rec(func)) {
        auto arg0 = func->get_args()[0];
        auto arg1 = func->get_args()[1];
        auto entry = func->get_entry_bb();
        entry->erase_inst(&entry->br_inst());
        auto mod = entry->create_inst<IBinaryInst>(
            IBinaryInst::SREM, arg1, Constants::get().int_const(2));
        auto exit = func->get_exit_bb();
        auto even_bb = func->create_bb();
        even_bb->create_inst<BrInst>(exit);
        auto odd_bb = func->create_bb();
        odd_bb->create_inst<BrInst>(exit);
        auto cond = entry->create_inst<ICmpInst>(ICmpInst::EQ, mod,
                                                 Constants::get().int_const(0));
        entry->create_inst<BrInst>(cond, even_bb, odd_bb);
        // erase old ret, leave phi for deadcode
        exit->erase_inst(&*exit->insts().rbegin());
        auto phi = exit->create_inst<PhiInst>(func->get_return_type());
        phi->from_pairs(
            {{arg0, even_bb}, {Constants::get().float_const(0), odd_bb}});
        exit->create_inst<RetInst>(phi);
    }
}