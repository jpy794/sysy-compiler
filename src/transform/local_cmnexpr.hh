#pragma once
#include "depth_order.hh"
#include "hash.hh"
#include "instruction.hh"
#include "pass.hh"
#include "value.hh"
#include <unordered_map>
#include <utility>
#include <vector>

namespace pass {

class LocalCmnExpr final : public pass::TransformPass {
  public:
    LocalCmnExpr() = default;
    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.add_require<DepthOrder>();
        AU.set_kill_type(KillType::Normal);
    }
    virtual bool run(pass::PassManager *mgr) override;

    // one OP and operands can uniquely represent common expressions
    enum class OP {
        ADD = 0,
        SUB,
        MUL,
        SDIV,
        SREM,
        XOR,
        LSHR,
        ASHR,
        SHL,
        FADD,
        FSUB,
        FMUL,
        FDIV,
        EQ,
        NE,
        GT,
        GE,
        LT,
        LE,
        FEQ,
        FNE,
        FGT,
        FGE,
        FLT,
        FLE,
        GEP,
        SI2FP,
        FP2SI,
        ZEXT
    };

    // check whether inst is required to calculate HASH_MAP
    inline bool check_inst(ir::Instruction *inst);

    // get OP for every inst by traversing its class type
    OP get_op(ir::Instruction *);

  private:
    bool changed;

    const DepthOrder::ResultType *depth_order;
    std::unordered_map<std::pair<OP, const std::vector<ir::Value *>>,
                       ir::Instruction *, PairHash>
        cmn_expr;
};

}; // namespace pass
