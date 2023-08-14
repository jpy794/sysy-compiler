#pragma once
#include "basic_block.hh"
#include "depth_order.hh"
#include "function.hh"
#include "module.hh"
#include "pass.hh"
#include "remove_unreach_bb.hh"
#include <map>
#include <set>

namespace pass {
class Dominator final : public pass::AnalysisPass {
  public:
    explicit Dominator() {}
    ~Dominator() = default;

    struct ResultType {
        std::map<ir::BasicBlock *, std::set<ir::BasicBlock *>> dom_frontier;
        std::map<ir::BasicBlock *, std::set<ir::BasicBlock *>>
            dom_tree_succ_blocks;
        bool is_dom(ir::BasicBlock *domer, ir::BasicBlock *domee) const;
    };

    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::None);
        AU.add_require<RmUnreachBB>();
        AU.add_require<DepthOrder>();
    }

    virtual bool run(pass::PassManager *mgr) override;

    virtual std::any get_result() const override { return &_result; }

    virtual void clear() override {
        _result.dom_frontier.clear();
        _result.dom_tree_succ_blocks.clear();
        _idom.clear();
        _depth_order = nullptr;
        f = nullptr;
    }

  private:
    ResultType _result;

    const pass::DepthOrder::ResultType *_depth_order;

    ir::Function *f;

    std::map<ir::BasicBlock *, ir::BasicBlock *> _idom{};

    ir::BasicBlock *intersect(ir::BasicBlock *b1, ir::BasicBlock *b2);
    void create_idom(ir::Function *f);
    void create_dominance_frontier(ir::Function *f);
    void create_dom_tree_succ(ir::Function *f);
};
} // namespace pass
