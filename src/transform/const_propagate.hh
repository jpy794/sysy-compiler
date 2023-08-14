#pragma once
#include "constant.hh"
#include "dead_code.hh"
#include "function.hh"
#include "instruction.hh"
#include "pass.hh"
#include "value.hh"
#include <deque>
#include <map>
#include <set>

namespace pass {

class ConstPro final : public pass::TransformPass {
  public:
    ConstPro() = default;
    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
        AU.add_post<pass::DeadCode>();
    }
    virtual bool run(pass::PassManager *mgr) override;

    void traverse(ir::Function *);
    void replace();
    ir::Constant *get_const(ir::Value *);
    ir::Constant *const_folder(ir::Instruction *);
    inline bool check(ir::Instruction *); // check whether inst is able to be
                                          // folded constantly

  private:
    std::set<ir::Instruction *> const_propa;
    std::map<ir::Value *, ir::Constant *> val2const;
    std::deque<ir::Instruction *> work_list{};
};

}; // namespace pass
