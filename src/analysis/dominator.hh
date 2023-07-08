#pragma once
#include "basic_block.hh"
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
    }

    virtual void run(pass::PassManager *mgr) override;

    virtual std::any get_result() const override { return &_result; }

    virtual void clear() override {
        _result.dom_frontier.clear();
        _result.dom_tree_succ_blocks.clear();
    }

  private:
    ResultType _result;

    std::list<ir::BasicBlock *> _depth_priority_order{};
    std::map<ir::BasicBlock *, int> _post_order_id{};
    std::map<ir::BasicBlock *, ir::BasicBlock *> _idom{};

    void post_order_visit(ir::BasicBlock *bb,
                          std::set<ir::BasicBlock *> &visited);
    ir::BasicBlock *intersect(ir::BasicBlock *b1, ir::BasicBlock *b2);

    void create_depth_priority_order(ir::Function *f);
    void create_idom(ir::Function *f);
    void create_dominance_frontier(ir::Function *f);
    void create_dom_tree_succ(ir::Function *f);
};
} // namespace pass