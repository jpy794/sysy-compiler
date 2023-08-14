#include "remove_unreach_bb.hh"
#include "basic_block.hh"
#include "err.hh"
#include "instruction.hh"
#include "utils.hh"
#include <cassert>
#include <deque>
#include <stdexcept>
#include <vector>

using namespace pass;
using namespace ir;
using namespace std;

bool RmUnreachBB::run(PassManager *mgr) {
    auto m = mgr->get_module();
    for (auto &f_r : m->functions()) {
        if (f_r.is_external)
            continue;
        deque<BasicBlock *> work_list;
        map<BasicBlock *, bool> visited;
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
        for (auto iter = f_r.bbs().begin(); iter != f_r.bbs().end();) {
            auto bb = &*iter;
            ++iter;
            if (visited[bb])
                continue;
            bb->replace_all_use_with_if(nullptr, [&](const Use &use) {
                return is_a<BrInst>(use.user);
            });
            while (bb->get_use_list().size()) {
                auto user = bb->get_use_list().begin()->user;
                assert(is_a<PhiInst>(user));
                as_a<PhiInst>(user)->rm_phi_param_from(bb, false);
            }
            f_r.bbs().erase(bb);
        }
    }
    return false;
}
