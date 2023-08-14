#pragma once
#include "func_info.hh"
#include "function.hh"
#include "instruction.hh"
#include "module.hh"
#include "pass.hh"
#include <deque>
#include <unordered_map>

namespace pass {

class DeadCode final : public pass::TransformPass {
  public:
    DeadCode() = default;
    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
        AU.add_require<pass::FuncInfo>();
    }
    virtual bool run(pass::PassManager *mgr) override;

  private:
    void mark_sweep(ir::Function *);
    void mark();
    void sweep(ir::Function *);
    void sweep_globally(ir::Module *);
    bool is_critical(ir::Instruction *);

    const pass::FuncInfo::ResultType *_func_info;

    bool changed;
    std::deque<ir::Instruction *> work_list{};
    std::unordered_map<ir::Instruction *, bool> marked{};
};

}; // namespace pass
