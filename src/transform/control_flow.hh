#pragma once
#include "depth_order.hh"
#include "dominator.hh"
#include "function.hh"
#include "inst_visitor.hh"
#include "instruction.hh"
#include "loop_find.hh"
#include "loop_simplify.hh"
#include "pass.hh"
#include "remove_unreach_bb.hh"
#include <vector>

namespace pass {

class ControlFlow final : public pass::TransformPass {
  public:
    ControlFlow() = default;

    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.add_require<DepthOrder>();
        AU.add_post<RmUnreachBB>();
        AU.add_kill<DepthOrder>();
        AU.add_kill<Dominator>();
        AU.add_kill<LoopSimplify>();
        AU.add_kill<LoopFind>();
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
    std::vector<ir::BasicBlock *> redd_bbs_to_del;
};

}; // namespace pass
