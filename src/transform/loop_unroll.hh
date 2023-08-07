#pragma once

#include "dead_code.hh"
#include "loop_find.hh"
#include "loop_invariant.hh"
#include "pass.hh"
#include "usedef_chain.hh"

namespace pass {

class LoopUnroll final : public TransformPass {
  public:
    LoopUnroll() = default;
    void get_analysis_usage(AnalysisUsage &AU) const final {
        using KillType = AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
        AU.add_require<LoopFind>();
        // require preheader
        AU.add_require<LoopInvariant>();
        AU.add_require<UseDefChain>();
        AU.add_post<DeadCode>();
    }
    void run(pass::PassManager *mgr) final;

  private:
    static constexpr int UNROLL_MAX = 1000;

    using LoopInfo = LoopFind::ResultType::LoopInfo;
    using FuncLoopInfo = LoopFind::ResultType::FuncLoopInfo;
    using UseDefRes = UseDefChain::ResultType;

    // the loop that has 1 header, 1 body, 1 latch and 1 exit
    struct SimpleLoopInfo {
        std::set<ir::BasicBlock *> bbs;
        ir::BasicBlock *header{nullptr}, *body{nullptr}, *exit{nullptr},
            *preheader{nullptr};
        ir::Value *ind_var{nullptr};
        ir::ConstInt *initial{nullptr}, *bound{nullptr}, *step{nullptr};
        ir::ICmpInst::ICmpOp icmp_op;
    };

    static std::optional<SimpleLoopInfo>
    parse_simple_loop(ir::BasicBlock *header, const LoopInfo &loop);

    static bool should_unroll(const SimpleLoopInfo &simple_loop);

    static void unroll_simple_loop(const SimpleLoopInfo &simple_loop,
                                   const UseDefRes &use_def);

    static void handle_func(ir::Function *func, const FuncLoopInfo &loops,
                            const UseDefRes &use_def);
};

}; // namespace pass
