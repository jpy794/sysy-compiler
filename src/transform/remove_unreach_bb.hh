#pragma once
#include "pass.hh"
#include <iostream>

namespace pass {

class RmUnreachBB final : public pass::TransformPass {
  public:
    RmUnreachBB() = default;
    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
    }
    virtual void run(pass::PassManager *mgr) override;

    virtual bool always_invalid() const override { return true; }

  private:
};

}; // namespace pass
