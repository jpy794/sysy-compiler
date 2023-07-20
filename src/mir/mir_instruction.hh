#pragma once

#include "ilist.hh"
#include "mir_memory.hh"
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
 * - order matters, used in will_write_register()
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
    ADDI,
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
};

class Instruction final : public ilist<Instruction>::node {
    friend class Label;

  private:
    const MIR_INST _opcode;
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
    void set_operand(unsigned i, Value *reg) {
        assert(i < _operands.size());
        assert(is_a<PhysicalRegister>(reg) or is_a<StatckObject>(reg));
        _operands[i] = reg;
    }

    void dump(std::ostream &os, const Context &context) const;
    // each instruction writes 1 register(def) at most
    // the orther operands are use
    bool will_write_register() const;
    bool is_branch_inst() const {
        static const std::array branch_list = {
            BEQ, BNE, BLT, BGE, Jump,
        };
        return contains(branch_list, _opcode);
    }
    bool is_load_store() const {
        static const std::array load_store_list = {SD, SW, LD, LW, FLW, FSW};
        return contains(load_store_list, _opcode);
    }
};

}; // namespace mir
