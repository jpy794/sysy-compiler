#pragma once
#include "basic_block.hh"
#include "instruction.hh"
#include "loop_find.hh"
#include "pass.hh"
#include "rm_useless_loop.hh"
#include "user.hh"
#include "value.hh"
#include <map>
#include <utility>

namespace pass {

class InductionExpr final : public pass::TransformPass {
  public:
    InductionExpr() = default;

    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::Normal);
        AU.add_require<LoopFind>();
        AU.add_post<RmUselessLoop>();
    }

    virtual bool run(pass::PassManager *mgr) override;

    // be used out of the loop
    bool is_induction_expr(ir::Instruction *);

    void induction(ir::Instruction *expr, const ir::Use &);

    // induction rules
    ir::Value *induce_add_rem(ir::Instruction *expr, ir::Instruction *user);

    // require loop_find to give these interfaces as follow
    // judge whether a bb is a loop head
    bool is_loop_head(ir::BasicBlock *bb) {
        return _func_loops->loop_info.at(bb->get_func()).loops.find(bb) !=
               _func_loops->loop_info.at(bb->get_func()).loops.end();
    }

    // judge whether user is out of the loop of loop_head
    bool out_of_loop(ir::BasicBlock *user, ir::BasicBlock *loop_head) {
        return not contains(_func_loops->loop_info.at(loop_head->get_func())
                                .loops.at(loop_head)
                                .bbs,
                            user);
    }

    // calculate the iter times of a loop
    ir::Value *get_iter_times(ir::Instruction *expr) {
        auto cal_iter_times =
            [&](const LoopFind::ResultType::LoopInfo::IndVarInfo &ind_var_info)
            -> ir::Value * {
            if (not ind_var_info.initial->is<ir::ConstInt>() or
                ind_var_info.initial->as<ir::ConstInt>()->val() != 0) {
                return nullptr;
            }
            if (ind_var_info.icmp_op != ir::ICmpInst::ICmpOp::LT) {
                return nullptr;
            }
            if (not ind_var_info.step->is<ir::ConstInt>() or
                ind_var_info.step->as<ir::ConstInt>()->val() != 1) {
                return nullptr;
            }
            return ind_var_info.bound;
        };
        for (auto &&[func, loops] : _func_loops->loop_info) {
            for (auto &&[header, loop] : loops.loops) {
                if (contains(loop.bbs, expr->get_parent())) {
                    if (not loop.ind_var_info.has_value()) {
                        return nullptr;
                    }
                    return cal_iter_times(loop.ind_var_info.value());
                }
            }
        }
        throw unreachable_error{};
    }

  private:
    bool changed;

    std::map<ir::Value *, std::pair<ir::User *, unsigned>> replace_table;

    const LoopFind::ResultType *_func_loops;
};

}; // namespace pass
