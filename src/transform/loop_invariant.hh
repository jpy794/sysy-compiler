#pragma once

#include "loop_find.hh"
#include "pass.hh"

namespace pass {

class LoopInvariant final : public pass::TransformPass {
  public:
    LoopInvariant() = default;
    void get_analysis_usage(pass::AnalysisUsage &AU) const final {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
        AU.add_require<pass::LoopFind>();
    }
    void run(pass::PassManager *mgr) final;

  private:
};

}; // namespace pass
