#include "loop_find.hh"
#include "dominator.hh"
#include "instruction.hh"
#include "log.hh"
#include "utils.hh"
#include <queue>
#include <unordered_map>

using namespace pass;
using namespace std;
using namespace ir;

void LoopFind::run(PassManager *mgr) {
    using LoopInfo = ResultType::LoopInfo;

    clear();

    _dom = &mgr->get_result<Dominator>();
    _m = mgr->get_module();

    for (auto &&func : _m->functions()) {
        if (func.is_external) {
            continue;
        }
        // for each bb in func to find all loops in func
        unordered_map<BasicBlock *, LoopInfo> loops;
        for (auto &&bb : func.bbs()) {
            // try find a loop using bb as header
            for (auto &&pre_bb : bb.pre_bbs()) {
                if (_dom->is_dom(&bb, pre_bb)) {
                    // found a latch pre_bb -> bb
                    if (not contains(loops, &bb)) {
                        // first time to find the header bb
                        loops.insert({&bb, LoopInfo{}});
                    }
                    auto &&loop = loops[&bb];
                    loop.latches.push_back(pre_bb);
                    loop.bbs.merge(find_bbs_by_latch(&bb, pre_bb));
                }
            }
            if (contains(loops, &bb)) {
                auto &loop = loops[&bb];

                // set preheader if exists
                vector<BasicBlock *> outer_pre_bbs;
                for (auto pre : bb.pre_bbs()) {
                    // prebb out of the loop that has only one
                    // sucbb (header)
                    if (not contains(loop.bbs, pre)) {
                        outer_pre_bbs.push_back(pre);
                    }
                }

                loop.preheader = nullptr;
                if (outer_pre_bbs.size() == 1 and
                    outer_pre_bbs.back()->suc_bbs().size() == 1) {
                    loop.preheader = outer_pre_bbs.back();
                }

                // find all exiting bbs
                // set exit bb for each exiting bb if exists
                for (auto bb : loop.bbs) {
                    for (auto suc : bb->suc_bbs()) {
                        if (not contains(loop.bbs, suc)) {
                            loop.exits.insert({bb, nullptr});
                            if (suc->pre_bbs().size() == 1) {
                                loop.exits[bb] = suc;
                            }
                        }
                    }
                }
            }
        }
        _result.loop_info[&func] = std::move(loops);
    }
    // log();
}

set<BasicBlock *> LoopFind::find_bbs_by_latch(BasicBlock *header,
                                              BasicBlock *latch) {
    set<BasicBlock *> ret;
    ret.insert(header);
    queue<BasicBlock *> bfs;
    bfs.push(latch);
    ret.insert(latch);
    while (not bfs.empty()) {
        auto bb = bfs.front();
        bfs.pop();
        for (auto &&pre_bb : bb->pre_bbs()) {
            if (not contains(ret, pre_bb)) {
                bfs.push(pre_bb);
                ret.insert(pre_bb);
            }
        }
    }
    return ret;
}

void LoopFind::log() const {
    for (auto &&[func, loops] : _result.loop_info) {
        debugs << func->get_name() << '\n';
        for (auto &&[header, loop] : loops) {
            debugs << "loop " << header->get_name() << '\n';
            for (auto &&latch : loop.latches) {
                debugs << latch->get_name() << ' ';
            }
            debugs << '\n';
            for (auto &&bb : loop.bbs) {
                debugs << bb->get_name() << ' ';
            }
            debugs << "\n\n";
        }
        debugs << '\n';
    }
}
