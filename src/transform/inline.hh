#pragma once
#include "const_propagate.hh"
#include "depth_order.hh"
#include "function.hh"
#include "ilist.hh"
#include "instruction.hh"
#include "pass.hh"
#include "remove_unreach_bb.hh"
#include "usedef_chain.hh"
#include "value.hh"
#include <deque>
#include <unordered_map>

namespace pass {

class Inline final : public pass::TransformPass {
  public:
    Inline() = default;
    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
        AU.add_require<pass::UseDefChain>();
        AU.add_require<DepthOrder>();
        AU.add_post<RmUnreachBB>();
        AU.add_post<ConstPro>();
    }

    virtual void run(pass::PassManager *mgr) override;

  private:
    bool is_inline(ir::Function *);
    void inline_func(ilist<ir::Instruction>::iterator);
    void clone(ir::Function *, ir::Function *);
    void replace(ilist<ir::Instruction>::iterator);
    void trivial(ilist<ir::Instruction>::iterator);
    std::unordered_map<const ir::Value *, ir::Value *> clee2cler;
    std::deque<ir::BasicBlock *> inline_bb;
    const pass::UseDefChain::ResultType *_use_def_chain;
};

}; // namespace pass
