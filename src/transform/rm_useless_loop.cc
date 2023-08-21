#include "rm_useless_loop.hh"
#include "basic_block.hh"
#include "err.hh"
#include "func_info.hh"
#include "instruction.hh"
#include "loop_find.hh"
#include "utils.hh"
#include <cassert>
#include <map>
#include <vector>

using namespace std;
using namespace ir;
using namespace pass;

bool RmUselessLoop::run(pass::PassManager *mgr) {
    auto m = mgr->get_module();
    func_loop = &mgr->get_result<LoopFind>();
    func_info = &mgr->get_result<FuncInfo>();
    bool changed = false;
    for (auto &f_r : m->functions()) {
        if (f_r.is_external)
            continue;
        auto &info = func_loop->loop_info.at(&f_r);
        auto tmp = info.get_topo_order();
        vector<BasicBlock *> reverse_order{tmp.rbegin(), tmp.rend()};
        list<BasicBlock *> rm_loops{};
        map<BasicBlock *, bool> bb_critical{};
        bool critical = false;
        for (auto loop_head : reverse_order) {
            // consider loop with break as critical
            if (info.loops.at(loop_head).exits.size() > 1)
                continue;
            for (auto loop_body : info.loops.at(loop_head).bbs) {
                if (loop_body == loop_head) {
                    // check if insts in loop_head aren't used out of loop
                    for (auto &inst : loop_head->insts()) {
                        if (is_a<BrInst>(&inst))
                            continue;
                        for (auto &use : inst.get_use_list()) {
                            critical |= out_of_loop(
                                as_a<Instruction>(use.user)->get_parent(),
                                loop_head);
                            if (critical)
                                cout << endl;
                        }
                    }
                } else {
                    critical |= is_critical(loop_body);
                }
            }
            if (not critical) {
                rm_loops.push_back(loop_head);
            }
        }
        while (not rm_loops.empty()) {
            auto top = rm_loops.front();
            rm_loops.pop_front();
            remove_loop(top);
            changed = true;
        }
    }
    return changed;
}

bool RmUselessLoop::is_critical(ir::BasicBlock *bb) {
    for (auto &inst_r : bb->insts()) {
        auto inst = &inst_r;
        if (is_a<StoreInst>(inst))
            return true;
        if (is_a<CallInst>(inst) && not func_info->is_pure_function(
                                        as_a<Function>(inst->operands()[0])))
            return true;
    }
    return false;
}

bool RmUselessLoop::out_of_loop(ir::BasicBlock *user,
                                ir::BasicBlock *loop_head) {
    return not contains(
        func_loop->loop_info.at(loop_head->get_func()).loops.at(loop_head).bbs,
        user);
}

void RmUselessLoop::remove_loop(ir::BasicBlock *head) {
    auto &info = func_loop->loop_info.at(head->get_func()).loops.at(head);
    auto pre_br = &info.preheader->br_inst();
    unsigned i;
    for (i = 0; i < pre_br->operands().size(); i++) {
        if (pre_br->get_operand(i) == head) {
            pre_br->set_operand(i, info.exits.at(head));
            break;
        }
    }
    assert(i != pre_br->operands().size());
    for (auto &inst_r : info.exits.at(head)->insts()) {
        if (is_a<PhiInst>(&inst_r)) {
            if (contains(inst_r.operands(),
                         static_cast<Value *>(info.preheader))) {
                as_a<PhiInst>(&inst_r)->rm_phi_param_from(head, false);
                continue;
            }
            for (unsigned i = 1; i < inst_r.operands().size(); i += 2) {
                if (inst_r.get_operand(i) == head) {
                    inst_r.set_operand(i, info.preheader);
                    break;
                }
            }
        } else
            break;
    }
    // FIXME:suc/pre bbs error
}