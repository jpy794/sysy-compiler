#pragma once

#include "ilist.hh"
#include "mir_context.hh"
#include "mir_value.hh"
#include <array>
#include <vector>

namespace mir {

/* RISCV RV64GC Subset
 * - support int operations only for now: RV32I RV64I RV64M
 * - abort instructions: unsigned related, non 32-bit,
 */
enum MIR_INST {
    /* RV32I */
    LUI,
    AUIPC,
    JAL,
    JALR,
    BEQ,
    BNE,
    BLT,
    BGE,
    // BLTU,
    // BGEU,
    LB,
    LH,
    LW,
    // LBU,
    // LHU,
    SB,
    SH,
    SW,
    // ADDI,
    SLTI,
    // SLTIU,
    XORI,
    ORI,
    ANDI,
    // SLLI,
    // SRLI,
    // SRAI,
    ADD,
    SUB,
    // SLL,
    SLT,
    // SLTU,
    XOR,
    // SRL,
    // SRA,
    OR,
    AND,
    // FENCE,
    // ECALL,
    // EBREAK,
    /* RV64I */
    // LWU,
    LD,
    SD,
    // SLLI,
    // SRLI,
    // SRAI,
    ADDIW,
    SLLIW,
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
    // DIVUW,
    REMW,
    // REMUW,
    /* pseudo instruction */
    Move,
    Jump,
    LoadAddress,
    LoadImmediate,
    SetEQZ,
    SetNEQZ,
    Call,
    Ret,
};

class Instruction final : public ilist<Instruction>::node {
    /* enum class R_IDX : unsigned { DEST, SRC1, SRC2 };
     * enum class I_IDX : unsigned { DEST, SRC1, IMM };
     * enum class S_IDX : unsigned { SRC, BASE, IMM };
     * enum class B_IDX : unsigned { SRC1, SRC2, LABEL };
     * enum class U_IDX : unsigned { DEST, IMM };
     * using J_IDX = U_IDX; */
  private:
    const MIR_INST _opcode;
    std::vector<Value *> _operands;
    bool _partial; // incomplete, or to say reserved for stage 2

  public:
    Instruction(MIR_INST opcode, std::vector<Value *> oprands, bool partial)
        : _opcode(opcode), _operands(oprands), _partial(partial) {}

    const Value *get_operand(unsigned i) const { return _operands.at(i); }
    Value *get_operand(unsigned i) { return _operands.at(i); }

    void dump(std::ostream &os, const MIRContext &context) const;

  private:
};

}; // namespace mir
