#pragma once

#include "const_propagate.hh"
#include "dead_code.hh"
#include "function.hh"
#include "global_variable.hh"
#include "mem2reg.hh"
#include "pass.hh"

#include <vector>

namespace pass {

class GlobalVarLocalize : public TransformPass {
    static bool NeedMem2reg;
    static bool NeedConstPro;

  public:
    bool run(PassManager *mgr) override;

    virtual void get_analysis_usage(AnalysisUsage &AU) const override {
        using KillType = AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
        if (NeedMem2reg)
            AU.add_post<Mem2reg>();
        if (NeedConstPro)
            AU.add_post<ConstPro>();
        AU.add_post<DeadCode>();
    }

  private:
    enum Action { BaseTypeSink, ConstArrProp, JustSkip };
    Action parse(ir::GlobalVariable *, ir::Function *main);

    // BaseTypeSink
    void sink(ir::GlobalVariable *);
    // ConstArrProp
    void prop_const(ir::GlobalVariable *);

    static inline ir::Type *global_type(ir::GlobalVariable *);
};

}; // namespace pass
