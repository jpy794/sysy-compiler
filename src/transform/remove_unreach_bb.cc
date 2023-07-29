#include "remove_unreach_bb.hh"
#include "basic_block.hh"
#include "utils.hh"
#include <cassert>
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
        auto &bbs = f_r.bbs();
        auto iter = bbs.begin();
        ++iter;
        vector<BasicBlock *> check_reach;
        while (iter != bbs.end()) {
            // find which block (not entry) doesn't have pre bb;
            if (iter->get_pre_bbs().size() == 0) {
                check_reach.push_back(&*iter);
            }
            ++iter;
        }
        // chech reachable and remove unreachable bbs
        for (unsigned i = 0; i < check_reach.size(); i++) {
            if (check_reach[i]->get_pre_bbs().size() == 0) { // unreachable
                for (auto suc_bb : check_reach[i]->suc_bbs()) {
                    auto &pre_suc_bbs = suc_bb->pre_bbs();
                    for (auto pre_suc_bb = pre_suc_bbs.begin();
                         pre_suc_bb != pre_suc_bbs.end(); pre_suc_bb++) {
                        if (*pre_suc_bb == check_reach[i]) {
                            // remove the unreach_bb from the ilist of the
                            // pre_bb of its suc_bbs
                            pre_suc_bbs.erase(pre_suc_bb);
                            break;
                        }
                    }
                    // add the unreach_bb's suc bb into check_reach to check
                    // whether it can be reached
                    check_reach.push_back(suc_bb);
                }
                // remove unreachable bb from functions
                for (iter = ++bbs.begin(); iter != bbs.end(); ++iter) {
                    if (&*iter == check_reach[i]) {
                        bbs.erase(iter);
                        break;
                    }
                }
            }
        }
    }
}