#pragma once

#include "ilist.hh"
#include "mir_value.hh"
#include "utils.hh"
#include <cstdint>
#include <string>
#include <vector>

namespace mir {

class Function;

/* RISCV RV64GC Subset
 * - support int operations only for now: RV32I RV64I RV64M
 * - abort instructions: unsigned related, non 32-bit(reserve on ptr calc case)
 * - order matters, used in will_write_register()
 */
enum MIR_INST : uint16_t {
    /* RV32I */
    LW = 0,
    SLTI,
    XORI,
    ORI,
    ANDI,
    ADD,
    SUB,
    SLT,
    XOR,
    OR,
    AND,
    SLLI,
    /* RV64I */
    LD,
    ADDI,
    ADDIW,
    SRLIW,
    SRAIW,
    ADDW,
    SUBW,
    SLLW,
    SRLW,
    SRAW,
    /* RV64M */
    MUL,
    MULW,
    DIVW,
    REMW,
    /* RV32F */
    FLW,   // flw f0, 0(x0)
    FADDS, // fadd.s f0, f1, f2
    FSUBS,
    FMULS,
    FDIVS,
    FCVTSW, // fcvt.s.w f0, x0
    FCVTWS, // fcvt.w.s x0, f0
    FMVXW,  // fmv.x.w x0, f0
    FMVWX,  // fmv.w.x f0, x0
    FLTS,   // flt.s x0, f0, f1
    FLES,
    FEQS,
    /* pseudo instruction */
    Move,
    FMove,
    LoadAddress,
    LoadImmediate,
    SetEQZ,
    SetNEQZ,
    Call,
    Jump,
    Ret,
    /* instructions that will not write register */
    BEQ,
    BNE,
    BLT,
    BGE,
    /* JAL,
     * JALR, */
    SW,
    SD,
    FSW, // fsw f0, 0(x0)
    /* special one */
    COMMENT,
};

class Comment : public Value {
    friend class ValueManager;
    Comment(std::string s) : _comment(s) {}
    std::string _comment;
    virtual void dump(std::ostream &os, const Context &context) const {
        os << _comment;
    }
};

class Instruction final : public ilist<Instruction>::node {
    friend class Label;

  private:
    MIR_INST _opcode;
    std::vector<Value *> _operands;

    // NOTE: aborted attribute
    bool _partial; // incomplete, or to say reserved for stage 2

  public:
    Instruction(MIR_INST opcode, std::vector<Value *> oprands, bool partial)
        : _opcode(opcode), _operands(oprands), _partial(partial) {}

    // start from 0
    decltype(_operands) &operands() { return _operands; }
    const Value *get_operand(unsigned i) const { return _operands.at(i); }
    Value *get_operand(unsigned i) { return _operands.at(i); }
    const size_t get_operand_num() const { return _operands.size(); }
    MIR_INST get_opcode() const { return _opcode; }
    void change_opcode(MIR_INST opcode) { _opcode = opcode; }
    void set_operand(unsigned i, Value *v);
    void degenerate_to_comment();

    void dump(std::ostream &os, const Context &context) const;
    // each instruction writes 1 register(def) at most
    // the orther operands are use
    bool will_write_register() const;
    bool is_branch_inst() const;
    bool is_load_store() const;
    bool should_round_towards_zero() const;
};

}; // namespace mir
