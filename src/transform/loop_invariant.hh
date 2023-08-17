#pragma once

#include "dominator.hh"
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
        AU.add_require<Dominator>();
    }
    bool run(PassManager *mgr) final;

  private:
    using LoopInfo = LoopFind::ResultType::LoopInfo;
    using FuncLoopInfo = LoopFind::ResultType::FuncLoopInfo;

    const Dominator::ResultType *_dom{nullptr};

    void handle_func(ir::Function *func, const FuncLoopInfo &func_loop);
    bool is_invariant_operand(ir::Value *op, const LoopInfo &loop);
    bool is_side_effect_inst(ir::Instruction *inst);
    bool is_dom_store(ir::Instruction *inst, const LoopInfo &loop);
    std::vector<ir::Instruction *> collect_invariant_inst(ir::BasicBlock *bb,
                                                          const LoopInfo &loop);
    std::vector<ir::Instruction *> collect_gep_store(ir::BasicBlock *bb,
                                                     const LoopInfo &loop);
};

}; // namespace pass
