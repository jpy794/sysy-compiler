#include "remove_unreach_bb.hh"
#include "utils.hh"
#include <cassert>
#include <stdexcept>

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
        while (iter != bbs.end()) {
            if (iter->get_pre_bbs().size() == 0) {
                for (auto suc_bb : iter->suc_bbs()) {
                    auto &pre_suc_bbs = suc_bb->pre_bbs();
                    for (auto pre_suc_bb = pre_suc_bbs.begin();
                         pre_suc_bb != pre_suc_bbs.end(); pre_suc_bb++) {
                        if (*pre_suc_bb == &*iter) {
                            pre_suc_bbs.erase(pre_suc_bb);
                            break;
                        }
                    }
                }
                iter = bbs.erase(iter);
            } else {
                ++iter;
            }
        }
    }
}