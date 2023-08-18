#include "remove_unreach_bb.hh"
#include "basic_block.hh"
#include "err.hh"
#include "instruction.hh"
#include "utils.hh"
#include <cassert>
#include <deque>
#include <map>
#include <stdexcept>
#include <vector>

using namespace pass;
using namespace ir;
using namespace std;

bool RmUnreachBB::run(PassManager *mgr) {
    auto m = mgr->get_module();
    bool changed = false;
    for (auto &f_r : m->functions()) {
        if (f_r.is_external)
            continue;
        deque<BasicBlock *> work_list{};
        deque<BasicBlock *> unreach_bbs{};
        map<BasicBlock *, bool> visited{};
        // mark the reachable bbs in BFS
        work_list.push_back(f_r.get_entry_bb());
        while (not work_list.empty()) {
            auto top = work_list.front();
            work_list.pop_front();
            if (visited[top])
                continue;
            visited[top] = true;
            for (auto suc_bb : top->suc_bbs()) {
                work_list.push_back(suc_bb);
            }
        }
        // remove the unreachable bbs with suc_bbs in reachable bbs in BFS
        work_list.push_back(f_r.get_entry_bb());
        map<BasicBlock *, bool> visited2s{};
        map<BasicBlock *, bool> del{};
        while (not work_list.empty()) {
            auto top = work_list.front();
            work_list.pop_front();
            if (visited2s[top])
                continue;
            visited2s[top] = true;
            // prevent iterator invalidation
            auto pre_bbs = top->pre_bbs();
            for (auto pre_bb : pre_bbs) {
                // pre_bb is an unreachable bb
                if (not visited[pre_bb]) {
                    if (del[pre_bb])
                        continue;
                    unreach_bbs.push_back(pre_bb);
                    del[pre_bb] = true;
                    // remove unreach_bbs from the deepest one of them
                    while (not unreach_bbs.empty()) {
                        auto rm_bb = unreach_bbs.front();
                        unreach_bbs.pop_front();
                        for (auto pre : rm_bb->pre_bbs()) {
                            if (not del[pre]) {
                                unreach_bbs.push_back(pre);
                                del[pre] = true;
                            }
                        }
                        // maybe rm_bb has been delete
                        remove_bb(rm_bb);
                        changed = true;
                    }
                }
            }
            for (auto suc_bb : top->suc_bbs()) {
                work_list.push_back(suc_bb);
            }
        }
        // remove the unreachable bbs without pre_bbs and suc_bbs
        unreach_bbs.clear();
        for (auto &bb : f_r.bbs()) {
            if (visited[&bb])
                continue;
            unreach_bbs.push_back(&bb);
        }
        while (not unreach_bbs.empty()) {
            auto rm_bb = unreach_bbs.front();
            unreach_bbs.pop_front();
            remove_bb(rm_bb);
            changed = true;
        }
    }
    return changed;
}

void RmUnreachBB::remove_bb(ir::BasicBlock *bb) {
    bb->replace_all_use_with_if(
        nullptr, [&](const Use &use) { return is_a<BrInst>(use.user); });
    while (bb->get_use_list().size()) {
        auto user = bb->get_use_list().begin()->user;
        assert(is_a<PhiInst>(user));
        as_a<PhiInst>(user)->rm_phi_param_from(bb, false);
    }
    bb->get_func()->erase_bb(bb);
}
