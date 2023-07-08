#pragma once
#include "dominator.hh"
#include "instruction.hh"
#include "pass.hh"
#include "usedef_chain.hh"
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
        AU.add_require<pass::UseDefChain>();
        AU.add_kill<pass::UseDefChain>();
    }
    virtual void run(pass::PassManager *mgr) override;

  private:
    void generate_phi(ir::Function *f);
    void re_name(ir::BasicBlock *bb);
    const pass::Dominator::ResultType *_dominator;
    const pass::UseDefChain::ResultType *_usedef_chain;

    std::set<std::pair<ir::Value *, ir::BasicBlock *>> _phi_table;
    std::map<ir::PhiInst *, ir::Value *> _phi_lval;
    std::map<ir::Value *, std::vector<ir::Value *>> _var_new_name;
};

}; // namespace pass
