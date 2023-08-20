#pragma once

#include "pass.hh"

namespace pass {

class PhiCombine final : public TransformPass {
  public:
    void get_analysis_usage(AnalysisUsage &AU) const final {
        using KillType = AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
    }
    bool run(PassManager *mgr) final;

  private:
    void handle_func(ir::Function *func);
    bool try_combine(ir::BasicBlock *bb, ir::BasicBlock *pre_bb);
};

}; // namespace pass