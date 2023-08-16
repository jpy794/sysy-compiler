#pragma once
#include "basic_block.hh"
#include "module.hh"
#include "pass.hh"
#include <map>
#include <set>

namespace pass {
class DepthOrder final : public pass::AnalysisPass {
  public:
    explicit DepthOrder() {}
    ~DepthOrder() = default;

    struct ResultType {
        std::map<ir::Function *, std::list<ir::BasicBlock *>>
            _depth_priority_order;
        std::map<ir::Function *, std::map<ir::BasicBlock *, int>>
            _post_order_id;
    };

    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::None);
    }

    virtual bool run(pass::PassManager *mgr) override;

    virtual std::any get_result() const override { return &_result; }

    virtual void clear() override {
        _result._depth_priority_order.clear();
        _result._post_order_id.clear();
    }

  private:
    ResultType _result;

    ir::Function *_func;

    void post_order_visit(ir::BasicBlock *bb,
                          std::set<ir::BasicBlock *> &visited);

    void create_depth_priority_order(ir::Function *f);
};
} // namespace pass
