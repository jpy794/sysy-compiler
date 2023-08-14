#include "depth_order.hh"

using namespace ir;
using namespace pass;

bool DepthOrder::run(PassManager *mgr) {
    clear();
    auto m = mgr->get_module();
    for (auto &f_r : m->functions()) {
        _func = &f_r;
        if (_func->is_external)
            continue;
        create_depth_priority_order(_func);
    }
    return false;
}

void DepthOrder::create_depth_priority_order(Function *f) {
    _result._depth_priority_order[f].clear();
    std::set<BasicBlock *> visited;
    post_order_visit(f->get_entry_bb(), visited);
    _result._depth_priority_order[f].reverse();
}

void DepthOrder::post_order_visit(BasicBlock *bb,
                                  std::set<BasicBlock *> &visited) {
    visited.insert(bb);
    for (auto b : bb->suc_bbs()) {
        if (visited.find(b) == visited.end())
            post_order_visit(b, visited);
    }
    _result._post_order_id[_func][bb] =
        _result._depth_priority_order[_func].size();
    _result._depth_priority_order[_func].push_back(bb);
}
