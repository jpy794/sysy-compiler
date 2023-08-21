#pragma once

#include "dead_code.hh"
#include "pass.hh"
#include "remove_unreach_bb.hh"

namespace pass {

class NaiveRecOpt final : public TransformPass {
  public:
    void get_analysis_usage(AnalysisUsage &AU) const final {
        using KillType = AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
        AU.add_post<DeadCode>();
        AU.add_post<RmUnreachBB>();
    }
    bool run(PassManager *mgr) final;

  private:
    void handle_func(ir::Function *func);
};

}; // namespace pass