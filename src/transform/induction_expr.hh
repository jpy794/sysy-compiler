#pragma once
#include "basic_block.hh"
#include "instruction.hh"
#include "pass.hh"
#include "value.hh"

namespace pass {

class InductionExpr final : public pass::TransformPass {
  public:
    InductionExpr() = default;

    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::Normal);
    }

    virtual bool run(pass::PassManager *mgr) override;

    // be used out of the loop
    bool is_induction_expr(ir::Instruction *);

    bool induction(ir::Instruction *expr, ir::Instruction *user);

    // induction rules
    bool add_rem(ir::Instruction *expr, ir::Instruction *user);

    // require loop_find to give these interfaces as follow
    // judge whether a bb is a loop head
    bool is_loop_head(ir::BasicBlock *); // TODO:

    // judge whether user is out of the loop of loop_head
    bool out_of_loop(ir::BasicBlock *user, ir::BasicBlock *loop_head); // TODO:

    // calculate the iter times of a loop
    ir::Value *get_iter_times(ir::Instruction *); // TODO:

  private:
};

}; // namespace pass
