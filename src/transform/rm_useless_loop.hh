#pragma once
#include "basic_block.hh"
#include "dead_code.hh"
#include "func_info.hh"
#include "instruction.hh"
#include "loop_find.hh"
#include "loop_simplify.hh"
#include "pass.hh"

namespace pass {

// when a loop body doesn't contain critical inst and the insts of the loop head
// just are used in the loop, the loop is useless
class RmUselessLoop final : public pass::TransformPass {

  public:
    RmUselessLoop() = default;
    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.add_require<LoopSimplify>();
        AU.add_require<LoopFind>();
        AU.add_require<FuncInfo>();
        AU.set_kill_type(KillType::All);
        AU.add_post<pass::DeadCode>();
    }

    bool run(pass::PassManager *mgr) override;

    bool is_critical(ir::BasicBlock *);

    bool out_of_loop(ir::BasicBlock *user, ir::BasicBlock *loop_head);

    void remove_loop(ir::BasicBlock *);

  private:
    const LoopFind::ResultType *func_loop;
    const FuncInfo::ResultType *func_info;
};

} // namespace pass
