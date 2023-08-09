#pragma once
#include "dead_code.hh"
#include "function.hh"
#include "instruction.hh"
#include "pass.hh"
#include "value.hh"
#include <cstddef>
#include <map>

namespace pass {

// only optimize continuous addition in the same bb
class ContinuousAdd final : public pass::TransformPass {
  public:
    ContinuousAdd() = default;
    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
        AU.add_post<pass::DeadCode>();
    }

    struct continuum {
        ir::Value *base;
        ir::Value *addend;
        size_t times;
    };

    virtual void run(pass::PassManager *mgr) override;

    void scan(ir::Function *);

    void create_add_con(ir::Instruction *);

    void add2mul(ir::Function *);

  private:
    std::map<ir::Value *, continuum> add_table;
    const unsigned add_upper_times = 4;
};

}; // namespace pass
