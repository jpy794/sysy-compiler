#include "inline.hh"
#include "depth_order.hh"
#include "function.hh"
#include "instruction.hh"
#include "type.hh"
#include "utils.hh"
#include <cassert>
#include <deque>
#include <iostream>

using namespace std;
using namespace pass;
using namespace ir;

bool Inline::is_inline(Function *func) {
    if (func->is_external)
        return false;
    return true;
}

bool Inline::run(PassManager *mgr) {
    auto m = mgr->get_module();
    const unsigned upper_times = 1; // set iter_expanded upper times
    deque<Instruction *> call_work_list{};
    unsigned iter_times = 0;
    Function *main_func;
    for (auto &f_r : m->functions()) {
        if (f_r.get_name() == "@main")
            main_func = &f_r;
    }
    while (iter_times++ < upper_times) { // find which call can be expanded
        for (auto &bb_r : main_func->bbs()) {
            for (auto iter = bb_r.insts().begin(); iter != bb_r.insts().end();
                 ++iter) {
                if (is_a<CallInst>(&*iter) &&
                    is_inline(as_a<Function>(iter->get_operand(0)))) {
                    call_work_list.push_back(&*iter);
                }
            }
        }
        if (call_work_list.empty())
            break;
        while (not call_work_list.empty()) {
            auto top = call_work_list.front();
            call_work_list.pop_front();
            clee2cler.clear();
            inline_bb.clear();
            inline_func(top);
        }
    }
    return false;
}

void Inline::inline_func(InstIter callee) {
    auto caller_func = callee->get_parent()->get_func();
    auto callee_func = as_a<Function>(callee->get_operand(0));
    // cast args to parameters
    auto &args = callee_func->get_args();
    for (unsigned i = 1; i < callee->operands().size(); i++) {
        clee2cler[args[i - 1]] = callee->get_operand(i);
    }
    clone(callee_func, caller_func);
    replace(callee);
    trivial(callee);
}

void Inline::clone(Function *callee, Function *caller) {
    auto entry_bb = caller->create_bb();
    clee2cler[callee->get_entry_bb()] = entry_bb;
    deque<BasicBlock *> bb_work_list;
    bb_work_list.push_back(callee->get_entry_bb());
    inline_bb.push_back(entry_bb);
    while (not bb_work_list.empty()) {
        auto top = bb_work_list.front();
        auto map_bb = as_a<BasicBlock>(clee2cler[top]);
        bb_work_list.pop_front();
        // step1 create successor bb to be inserted
        for (auto suc_bb : top->suc_bbs()) {
            if (clee2cler[suc_bb] == nullptr) {
                auto bb = caller->create_bb();
                clee2cler[suc_bb] = bb;
                bb_work_list.push_back(suc_bb);
                inline_bb.push_back(bb);
            }
        }
        // step2 clone inst from callee function into caller function
        for (auto &inst_r : top->insts()) {
            auto inst = &inst_r;
            auto map_inst =
                map_bb->clone_inst(map_bb->insts().end(), inst, true);
            clee2cler[inst] = map_inst;
        }
    }
}

void Inline::replace(InstIter call_iter) { // replace operands of inst
                                           // with map_val because the
    // operands now belong to callee function
    for (auto bb : inline_bb) {
        for (auto &inst_r : bb->insts()) {
            for (unsigned i = 0; i < inst_r.operands().size(); i++) {
                auto oper = inst_r.get_operand(i);
                if (is_a<Argument>(oper) || is_a<BasicBlock>(oper) ||
                    is_a<Instruction>(oper)) {
                    assert(clee2cler[oper]);
                    inst_r.set_operand(i, clee2cler[oper]);
                }
                // for constant, global and function, it shouldn't be replaced
            }
        }
    }
    auto callee_func = as_a<Function>(call_iter->get_operand(0));
    auto callee_exit_bb = callee_func->get_exit_bb();
    auto map_exit_bb = as_a<BasicBlock>(clee2cler[callee_exit_bb]);
    assert(is_a<RetInst>(&map_exit_bb->insts().back()));
    if (is_a<VoidType>(callee_func->get_return_type()))
        return;
    // replace call result with return val
    call_iter->replace_all_use_with(map_exit_bb->insts().back().get_operand(0));
}

void Inline::trivial(InstIter call_iter) {
    // step1 erase return inst within map_exit_bb
    auto callee = as_a<Function>(call_iter->get_operand(0));
    auto map_exit_bb = as_a<BasicBlock>(clee2cler[callee->get_exit_bb()]);
    map_exit_bb->erase_inst(&map_exit_bb->insts().back());
    // step2 move insts after call_inst from parent_bb to map_exit_bb
    auto parent_bb = call_iter->get_parent();
    auto move_iter = call_iter;
    // count which instructions need to be moved
    list<Instruction *> move_insts;
    for (++move_iter; move_iter != parent_bb->insts().end(); ++move_iter) {
        move_insts.push_back(&*move_iter);
    }
    for (auto inst : move_insts) {
        map_exit_bb->move_inst(map_exit_bb->insts().end(), inst);
    }
    // modify phi's source according to current bb's relationship
    for (auto br_tar_bb : map_exit_bb->suc_bbs()) {
        for (auto &inst_r : br_tar_bb->insts()) {
            if (is_a<PhiInst>(&inst_r)) {
                for (unsigned i = 1; i < inst_r.operands().size(); i += 2) {
                    if (inst_r.operands()[i] == parent_bb) {
                        inst_r.set_operand(i, map_exit_bb);
                    }
                }
            } else
                break;
        }
    }
    // step3 replace call_inst with a jump inst to map_entry_bb
    auto map_entry_bb = as_a<BasicBlock>(clee2cler[callee->get_entry_bb()]);
    parent_bb->erase_inst(&*call_iter);
    parent_bb->create_inst<BrInst>(map_entry_bb);
}
