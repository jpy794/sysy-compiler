#pragma once
#include "basic_block.hh"
#include "dead_code.hh"
#include "pass.hh"
#include <iostream>

namespace pass {

class RmUnreachBB final : public pass::TransformPass {
  public:
    RmUnreachBB() = default;
    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
        AU.add_post<DeadCode>();
    }
    virtual bool run(pass::PassManager *mgr) override;

    void remove_bb(ir::BasicBlock *);

  private:
};

}; // namespace pass
