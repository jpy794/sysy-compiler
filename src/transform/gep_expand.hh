#pragma once

#include "dead_code.hh"
#include "instruction.hh"
#include "loop_invariant.hh"
#include "pass.hh"

#include <vector>

namespace pass {

class GEP_Expand : public TransformPass {
  public:
    bool run(PassManager *mgr) override;

    virtual void get_analysis_usage(AnalysisUsage &AU) const override {
        using KillType = AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
        AU.add_post<DeadCode>();
        AU.add_post<LoopInvariant>();
    }

  private:
    bool expand(ir::GetElementPtrInst *);
};

}; // namespace pass
