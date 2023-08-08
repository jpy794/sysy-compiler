#pragma once

#include "loop_find.hh"
#include "loop_simplify.hh"
#include "pass.hh"

namespace pass {

class LoopInvariant final : public TransformPass {
  public:
    void get_analysis_usage(AnalysisUsage &AU) const final {
        using KillType = AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
        AU.add_require<LoopSimplify>();
        AU.add_require<LoopFind>();
    }
    void run(PassManager *mgr) final;

    bool always_invalid() const override { return true; }

  private:
    using LoopInfo = LoopFind::ResultType::LoopInfo;
    using FuncLoopInfo = LoopFind::ResultType::FuncLoopInfo;

    static void handle_func(ir::Function *func, const FuncLoopInfo &loops);
    static bool is_invariant_operand(ir::Value *op, const LoopInfo &loop);
    static bool is_side_effect_inst(ir::Instruction *inst);
    static std::vector<ir::Instruction *>
    collect_invariant_inst(ir::BasicBlock *bb, const LoopInfo &loop);
};

}; // namespace pass
