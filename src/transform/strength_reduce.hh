#pragma once
#include "DeadCode.hh"
#include "pass.hh"

namespace pass {

class StrengthReduce final : public pass::TransformPass {

  public:
    StrengthReduce() = default;
    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
        AU.add_post<pass::DeadCode>();
    }
    void run(pass::PassManager *mgr) override;
    bool always_invalid() const override { return true; }

  private:
    // opt for: continuous add const
    void combine_continuous_add(ir::Function *);
    // opt: mul/div then add together
    void algebraic_combine(ir::Function *); // NOTE this may have overflow bug
};

} // namespace pass
