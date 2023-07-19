#include "dominator.hh"
#include "basic_block.hh"
#include "utils.hh"
#include <cassert>
#include <queue>
using namespace ir;
using namespace pass;

void Dominator::run(PassManager *mgr) {
    _depth_order = &mgr->get_result<DepthOrder>();
    clear();
    auto m = mgr->get_module();
    for (auto &f_r : m->functions()) {
        f = &f_r;
        if (f->is_external)
            continue;
        // init
        for (auto &bb_r : f->bbs()) {
            auto bb = &bb_r;
            _idom.insert({bb, nullptr});
            _result.dom_frontier.insert({bb, {}});
            _result.dom_tree_succ_blocks.insert({bb, {}});
        }

        create_idom(f);
        create_dominance_frontier(f);
        create_dom_tree_succ(f);
    }
}

void Dominator::create_idom(Function *f) {
    auto root = f->get_entry_bb();
    _idom[root] = root;

    bool changed = true;
    unsigned exc_times = 0;
    while (changed) {
        exc_times++;
        if (exc_times > 2) {
            // I think the method working out the idom of bb in a depth-first
            // order can excute once to get an right solution(has been verified
            // for non-goto)
            assert(false);
            // FIXME:after testing, this can be deleted.
        }
        changed = false;
        for (auto bb : _depth_order->_depth_priority_order.at(f)) {
            if (bb == root) {
                continue;
            }
            BasicBlock *pred;
            for (auto p : bb->pre_bbs()) {
                if (_idom[p]) {
                    pred = p;
                    break;
                }
            }
            assert(pred);
            // get the common pred of all pre_bb of current node
            //(the idom of current node)
            BasicBlock *new_idom = pred;
            for (auto p : bb->pre_bbs()) {
                if (_idom[p]) { // _idom[p]==nullptr means that p comes after bb
                                // in depth-first order
                    new_idom = intersect(p, new_idom);
                }
            }
            assert(new_idom);
            if (_idom[bb] != new_idom) {
                _idom[bb] = new_idom;
                changed = true;
            }
        }
    }
}

BasicBlock *Dominator::intersect(BasicBlock *b1, BasicBlock *b2) {
    // for nullptr, return the non-nullptr value
    if (not b1)
        return b2;
    if (not b2)
        return b1;
    while (b1 != b2) {
        // bb with a lower post_order_id is a deeper or equal depth node in CFG
        while (_depth_order->_post_order_id.at(f).at(b1) <
               _depth_order->_post_order_id.at(f).at(b2)) {
            assert(_idom[b1]);
            b1 = _idom[b1];
        }
        while (_depth_order->_post_order_id.at(f).at(b2) <
               _depth_order->_post_order_id.at(f).at(b1)) {
            assert(_idom[b2]);
            b2 = _idom[b2];
        }
    }
    return b1;
}

void Dominator::create_dominance_frontier(Function *f) {
    for (auto &bb_r : f->bbs()) {
        auto bb = &bb_r;
        if (bb->pre_bbs().size() >= 2) {
            for (auto p : bb->pre_bbs()) {
                auto runner = p;
                while (runner != _idom[bb]) {
                    assert(runner);
                    _result.dom_frontier[runner].insert(bb);
                    runner = _idom[runner];
                }
            }
        }
    }
}

void Dominator::create_dom_tree_succ(Function *f) {
    for (auto &bb_r : f->bbs()) {
        auto bb = &bb_r;
        auto idom = _idom[bb];
        if (idom != bb) {
            _result.dom_tree_succ_blocks[idom].insert(bb);
        }
    }
}

bool Dominator::ResultType::is_dom(BasicBlock *domer, BasicBlock *domee) const {
    std::queue<BasicBlock *> bfs{};
    bfs.push(domer);
    while (not bfs.empty()) {
        auto bb = bfs.front();
        bfs.pop();
        assert(contains(dom_tree_succ_blocks, bb));
        for (auto &&suc_bb : dom_tree_succ_blocks.at(bb)) {
            if (suc_bb == domee) {
                return true;
            }
            bfs.push(suc_bb);
        }
    }
    return false;
}
