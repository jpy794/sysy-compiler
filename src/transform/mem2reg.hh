#pragma once
#include "dead_code.hh"
#include "dominator.hh"
#include "instruction.hh"
#include "pass.hh"
#include "value.hh"
#include <iostream>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace pass {

class Mem2reg final : public pass::TransformPass {
  public:
    Mem2reg() = default;
    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
        AU.add_require<pass::Dominator>();
        AU.add_post<DeadCode>();
    }
    virtual void run(pass::PassManager *mgr) override;
    bool always_invalid() const override { return true; }

  private:
    void generate_phi(ir::Function *f);
    void re_name(ir::BasicBlock *bb);
    const pass::Dominator::ResultType *_dominator;

    bool is_wanted_phi(ir::Value *);
    void clear();

    std::set<std::pair<ir::Value *, ir::BasicBlock *>> _phi_table;
    std::map<ir::PhiInst *, ir::Value *> _phi_lval;
    std::map<ir::Value *, std::vector<ir::Value *>> _var_new_name;
};

}; // namespace pass
