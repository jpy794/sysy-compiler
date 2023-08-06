#include "remove_unreach_bb.hh"
#include "basic_block.hh"
#include "utils.hh"
#include <cassert>
#include <deque>
#include <stdexcept>
#include <vector>

using namespace pass;
using namespace ir;
using namespace std;

void RmUnreachBB::run(PassManager *mgr) {
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
        for (auto &bb : f_r.bbs()) {
            if (visited[&bb])
                continue;
            else {
                f_r.bbs().erase(&bb);
            }
        }
    }
}
