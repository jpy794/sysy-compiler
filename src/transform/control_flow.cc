#include "control_flow.hh"
#include "basic_block.hh"
#include "constant.hh"
#include "depth_order.hh"
#include "err.hh"
#include "function.hh"
#include "ilist.hh"
#include "instruction.hh"
#include "utils.hh"
#include <cassert>
#include <utility>

using namespace std;
using namespace pass;
using namespace ir;

void ControlFlow::run(pass::PassManager *mgr) {
    _depth_order = &mgr->get_result<DepthOrder>();
    auto m = mgr->get_module();
    for (auto &f : m->functions()) {
        if (f.is_external)
            continue;
        post_order = _depth_order->_depth_priority_order.at(&f);
        post_order.reverse();
        clean(&f);
    }
}

void ControlFlow::clean(ir::Function *func) {
    redd_bbs_to_del.clear();
    for (auto bb : post_order) {
        if (bb == func->get_entry_bb())
            continue;
        auto inst = &bb->insts().back();
        if (is_a<BrInst>(inst)) {
            if (is_branch(inst)) {
                auto TBB = as_a<BasicBlock>(inst->operands()[1]);
                auto FBB = as_a<BasicBlock>(inst->operands()[2]);
                if (TBB == FBB) {
                    BasicBlock *target_bb = nullptr;
                    if (TBB == FBB)
                        target_bb = TBB;
                    bb->erase_inst(inst);
                    bb->create_inst<BrInst>(target_bb);
                }
            }
            inst = &bb->insts().back();
            if (is_jump(inst)) {
                auto ToBB = as_a<BasicBlock>(inst->operands()[0]);
                if (bb->insts().size() ==
                    1) { // size==1 means that the bb is empty
                    merge_bb(bb, ToBB, func);
                } else if (ToBB->pre_bbs().size() == 1) {
                    merge_bb(bb, ToBB, func);
                } else if (ToBB->insts().size() == 1 and
                           is_branch(&ToBB->insts().back())) {
                    // rewrite bb'jump with tobb's branch
                    bb->erase_inst(inst);
                    auto bb_branch = ToBB->insts().back().clone(bb);
                    bb->clone_inst(bb->insts().end(), bb_branch);
                }
            }
        }
    }
    for (auto redd_bb : redd_bbs_to_del)
        func->bbs().erase(redd_bb);
}

// regard result_bb as the result merged bb by default
void ControlFlow::merge_bb(BasicBlock *redd_bb, BasicBlock *result_bb,
                           Function *func) {
    auto &pre_bbs = redd_bb->pre_bbs();
    if (is_a<PhiInst>(&result_bb->insts().front())) {
        for (auto pre_res_bb : result_bb->pre_bbs()) {
            for (auto pre_red_bb : pre_bbs) {
                // for phi op1-bb1, op2-bb2, op3-bb3...
                // merge will add op4-bb4, and if bb4 equal to any of phi's
                // bbs， it'll be confusing for backend
                if (pre_res_bb == pre_red_bb)
                    return;
            }
        }
    }
    // 1.correct special operands changed by variation of bb
    if (pre_bbs.size() > 0) {
        // replace redundant_bb in PhiInst of result bb with pre_bb of
        // redundant_bb
        for (auto &inst_r : result_bb->insts()) {
            if (is_a<PhiInst>(&inst_r)) {
                for (unsigned i = 1; i < inst_r.operands().size(); i += 2) {
                    if (inst_r.operands()[i] == redd_bb) {
                        inst_r.set_operand(i, *pre_bbs.begin());
                        for (auto iter = ++pre_bbs.begin();
                             iter != pre_bbs.end(); iter++) {
                            as_a<PhiInst>(&inst_r)->add_phi_param(
                                inst_r.operands()[i - 1], *iter);
                        }
                    }
                }
            } else
                break;
        }
        // replace redundant_bb in BrInst of predecessor of redundant bb with
        // result_bb
        list<pair<Instruction *, unsigned>> rep_br_inst;
        for (auto pre_bb : pre_bbs) {
            auto &br = pre_bb->insts().back();
            for (unsigned i = 0; i < br.operands().size(); i++) {
                if (br.operands()[i] == redd_bb) {
                    rep_br_inst.push_back({&br, i});
                }
            }
        }
        for (auto [br, i] : rep_br_inst) {
            br->set_operand(i, result_bb);
        }
    }
    // 2.remove BrInst of redundant bb
    redd_bb->erase_inst(&redd_bb->insts().back());
    // 3.move redundant bb's insts into result bb's begin position
    auto insert_iter = result_bb->insts().begin();
    for (; is_a<PhiInst>(&*insert_iter);
         ++insert_iter) // PhiInst should all be in the front of BB
        ;
    for (auto iter = redd_bb->insts().begin(); iter != redd_bb->insts().end();
         iter = redd_bb->insts().begin()) {
        assert(not(is_a<BrInst>(&*iter) || is_a<RetInst>(&*iter)));
        insert_iter = result_bb->move_inst(insert_iter, &*iter);
        ++insert_iter;
    }
    // 4.record redundant bb
    redd_bbs_to_del.push_back(redd_bb);
}

bool ControlFlow::is_branch(ir::Instruction *inst) {
    return is_a<BrInst>(inst) && not is_a<BasicBlock>(inst->get_operand(0));
}
bool ControlFlow::is_jump(ir::Instruction *inst) {
    return is_a<BrInst>(inst) && is_a<BasicBlock>(inst->get_operand(0));
}
