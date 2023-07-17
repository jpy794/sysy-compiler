#pragma once

#include "ilist.hh"
#include "mir_register.hh"
#include "mir_value.hh"
#include "utils.hh"
#include <array>
#include <cassert>
#include <vector>

namespace mir {

class Function;

/* RISCV RV64GC Subset
 * - support int operations only for now: RV32I RV64I RV64M
 * - abort instructions: unsigned related, non 32-bit(reserve on ptr calc case)
 * - order matters!
 */
enum MIR_INST {
    /* RV32I */
    LW,
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
    /* RV64I */
    LD,
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
    REMW,
    /* pseudo instruction */
    Move,
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
    JAL,
    JALR,
    SW,
    SD,
};

class Instruction final : public ilist<Instruction>::node {
    friend class Label;

  private:
    const MIR_INST _opcode;
    std::vector<Value *> _operands;
    bool _partial; // incomplete, or to say reserved for stage 2

  public:
    Instruction(MIR_INST opcode, std::vector<Value *> oprands, bool partial)
        : _opcode(opcode), _operands(oprands), _partial(partial) {}

    // start from 0
    const Value *get_operand(unsigned i) const { return _operands.at(i); }
    Value *get_operand(unsigned i) { return _operands.at(i); }
    const size_t get_operand_num() const { return _operands.size(); }
    void set_operand(unsigned i, PhysicalRegister *reg) {
        assert(i < _operands.size());
        _operands[i] = reg;
    }

    void dump(std::ostream &os, const Context &context) const;
    // each instruction writes 1 register(def) at most
    // the orther operands are use
    bool will_write_register() const;
    bool is_branch_inst() const {
        static const std::array branch_list = {
            JAL, JALR, BEQ, BNE, BLT, BGE, Jump,
        };
        return contains(branch_list, _opcode);
    }
};

}; // namespace mir
