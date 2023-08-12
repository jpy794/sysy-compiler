#pragma once
#include "const_propagate.hh"
#include "dead_code.hh"
#include "depth_order.hh"
#include "function.hh"
#include "global_localize.hh"
#include "ilist.hh"
#include "instruction.hh"
#include "loop_invariant.hh"
#include "pass.hh"
#include "remove_unreach_bb.hh"
#include "value.hh"
#include <deque>
#include <unordered_map>

namespace pass {

class Inline final : public pass::TransformPass {
  public:
    Inline() = default;

    using InstIter = ilist<ir::Instruction>::iterator;

    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
        AU.add_require<DepthOrder>();
        AU.add_post<DeadCode>();
        AU.add_post<GlobalVarLocalize>();
        AU.add_post<ConstPro>();
    }

    virtual void run(pass::PassManager *mgr) override;

  private:
    bool is_inline(ir::Function *);
    void inline_func(InstIter);
    void clone(ir::Function *, ir::Function *);
    void replace(InstIter);
    void trivial(InstIter);
    std::unordered_map<const ir::Value *, ir::Value *> clee2cler;
    std::deque<ir::BasicBlock *> inline_bb;
};

}; // namespace pass
