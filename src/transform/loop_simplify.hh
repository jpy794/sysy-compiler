#pragma once

#include "loop_find.hh"
#include "pass.hh"

namespace pass {

class LoopSimplify final : public TransformPass {
  public:
    void get_analysis_usage(AnalysisUsage &AU) const final {
        using KillType = AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
        AU.add_require<LoopFind>();
    }
    void run(PassManager *mgr) final;

  private:
    using LoopInfo = LoopFind::ResultType::LoopInfo;
    using FuncLoopInfo = LoopFind::ResultType::FuncLoopInfo;

    using Pair = ir::PhiInst::Pair;

    static std::pair<std::vector<Pair>, std::vector<Pair>>
    split_phi_op(ir::PhiInst *phi, const LoopInfo &loop);

    static void handle_func(ir::Function *func, const FuncLoopInfo &func_loop);

    static ir::BasicBlock *create_preheader(ir::BasicBlock *header,
                                            const LoopInfo &loop);

    static void create_exit(ir::BasicBlock *exiting,
                            ir::BasicBlock *exit_target);
};

}; // namespace pass