#pragma once
#include "depth_order.hh"
#include "function.hh"
#include "inst_visitor.hh"
#include "instruction.hh"
#include "pass.hh"
#include "remove_unreach_bb.hh"
#include "usedef_chain.hh"

namespace pass {

class ControlFlow final : public pass::TransformPass {
  public:
    ControlFlow() = default;
    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.add_require<DepthOrder>();
        AU.add_post<RmUnreachBB>();
        AU.set_kill_type(KillType::Normal);
    }

    virtual void run(pass::PassManager *mgr) override;

    virtual bool always_invalid() const override { return true; }

    void clean(ir::Function *);

    void merge_bb(ir::BasicBlock *, ir::BasicBlock *, ir::Function *);

    bool is_branch(ir::Instruction *);

    bool is_jump(ir::Instruction *);

  private:
    const DepthOrder::ResultType *_depth_order;
    std::list<ir::BasicBlock *> post_order;
};

}; // namespace pass
