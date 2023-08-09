#pragma once
#include "function.hh"
#include "instruction.hh"
#include "pass.hh"
#include "utils.hh"
#include <deque>
#include <set>

namespace pass {
class FuncInfo final : public pass::AnalysisPass {
  public:
    explicit FuncInfo() {}
    ~FuncInfo() = default;

    struct ResultType {
        std::set<ir::Function *> pure_functions{};
        std::map<ir::Function *, std::set<ir::Function *>> callers{};

        bool is_pure_function(ir::Function *inst) const {
            return contains(pure_functions, inst);
        }
    };

    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::None);
    }

    virtual std::any get_result() const override { return &_result; }

    virtual void run(pass::PassManager *mgr) override;

    virtual void clear() override {
        _result.pure_functions.clear();
        _result.callers.clear();
        callee_pure.clear();
    }

  private:
    bool maybe_pure(ir::Function *func);
    bool is_side_effect_inst(ir::Instruction *inst);
    ir::Value *get_origin_addr(ir::Value *addr);

    ResultType _result;

    std::deque<ir::Function *> callee_pure;
};
} // namespace pass
