#include "control_flow.hh"
#include "depth_order.hh"
#include "err.hh"
#include "function.hh"
#include "ilist.hh"
#include "instruction.hh"
#include "usedef_chain.hh"
#include "utils.hh"
#include <cassert>

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
    for (auto bb : post_order) {
        if (bb == func->get_entry_bb())
            continue;
        auto inst = &bb->insts().back();
        if (is_a<BrInst>(inst)) {
            if (is_branch(inst)) {
                auto TBB = as_a<BasicBlock>(inst->operands()[1]);
                auto FBB = as_a<BasicBlock>(inst->operands()[2]);
                if (TBB == FBB) {
                    bb->erase_inst(inst);
                    bb->create_inst<BrInst>(TBB);
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
}

// regard result_bb as the result merged bb by default
void ControlFlow::merge_bb(BasicBlock *redd_bb, BasicBlock *result_bb,
                           Function *func) {
    auto &pre_bbs = redd_bb->pre_bbs();
    if (is_a<PhiInst>(&result_bb->insts().front())) {
        for (auto pre_res_bb : result_bb->pre_bbs()) {
            for (auto pre_red_bb : pre_bbs) {
                // for phi op1-bb1, op2-bb2, op3-bb3...
                // merge will add op4-bb4, and if bb4 equal to one of phi's
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
                        inst_r.operands()[i] = pre_bbs[0];
                        for (unsigned j = 1; j < pre_bbs.size(); j++) {
                            as_a<PhiInst>(&inst_r)->add_phi_param(
                                inst_r.operands()[i - 1], pre_bbs[j]);
                        }
                    }
                }
            } else
                break;
        }
        // replace redundant_bb in BrInst of predecessor of redundant bb with
        // result_bb
        for (auto pre_bb : pre_bbs) {
            auto &br = pre_bb->insts().back();
            for (unsigned i = 0; i < br.operands().size(); i++) {
                if (br.operands()[i] == redd_bb) {
                    br.operands()[i] = result_bb;
                }
            }
        }
    }
    // 2.place predecessors of redundant bb into predecessor of result bb
    result_bb->pre_bbs().insert(result_bb->pre_bbs().end(), pre_bbs.begin(),
                                pre_bbs.end());
    // 3.change successor bbs of predecessor bbs of redundant bb from bb1 to bb2
    for (unsigned i = 0; i < pre_bbs.size(); i++) {
        auto &pre_suc_bbs = pre_bbs[i]->suc_bbs();
        for (unsigned j = 0; j < pre_suc_bbs.size(); j++) {
            if (pre_suc_bbs[j] == redd_bb) {
                pre_suc_bbs[j] = result_bb;
                break;
            }
        }
    }
    // 4.remove BrInst of redundant bb
    redd_bb->erase_inst(&redd_bb->insts().back());
    // 5.move redundant bb's insts into result bb's begin position
    auto insert_iter = result_bb->insts().begin();
    for (; is_a<PhiInst>(&*insert_iter);
         ++insert_iter) // PhiInst should all be in the front of BB
        ;
    for (auto iter = redd_bb->insts().begin(); iter != redd_bb->insts().end();
         iter = redd_bb->insts().begin()) {
        assert(not(is_a<BrInst>(&*iter) || is_a<RetInst>(&*iter)));
        auto inst = redd_bb->insts().release(iter);
        insert_iter = result_bb->insts().insert(insert_iter, inst);
        ++insert_iter;
    }
    // 6.clear redundant bb
    func->bbs().erase(redd_bb);
}

bool ControlFlow::is_branch(ir::Instruction *inst) {
    return is_a<BrInst>(inst) && is_a<Instruction>(inst->get_operand(0));
}
bool ControlFlow::is_jump(ir::Instruction *inst) {
    return is_a<BrInst>(inst) && is_a<BasicBlock>(inst->get_operand(0));
}
