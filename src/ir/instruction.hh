#pragma once

#include "ilist.hh"
#include "user.hh"

#include <array>

namespace ir {

class BasicBlock;

class Instruction : public User, public ilist<Instruction>::node {
  public:
    enum OpID {
        // Terminator Instructions
        ret,
        br,
        // Standard binary operators
        add,
        sub,
        mul,
        sdiv,
        // float binary operators
        fadd,
        fsub,
        fmul,
        fdiv,
        // Memory operators
        alloca,
        load,
        store,
        // Other operators
        cmp,
        fcmp,
        phi,
        call,
        getelementptr,
        fptosi,
        sitofp
    };

    Instruction(BasicBlock *bb, Type *type, OpID id,
                std::vector<Value *> &&operands);

    bool is_ret() const { return _id == ret; }
    bool is_br() const { return _id == br; }
    bool is_add() const { return _id == add; }
    bool is_sub() const { return _id == sub; }
    bool is_mul() const { return _id == mul; }
    bool is_sdiv() const { return _id == sdiv; }
    bool is_fadd() const { return _id == fadd; }
    bool is_fsub() const { return _id == fsub; }
    bool is_fmul() const { return _id == fmul; }
    bool is_fdiv() const { return _id == fdiv; }
    bool is_alloca() const { return _id == alloca; }
    bool is_load() const { return _id == load; }
    bool is_store() const { return _id == store; }
    bool is_cmp() const { return _id == cmp; }
    bool is_fcmp() const { return _id == fcmp; }
    bool is_phi() const { return _id == phi; }
    bool is_call() const { return _id == call; }
    bool is_getelementptr() const { return _id == getelementptr; }
    bool is_fptosi() const { return _id == fptosi; }
    bool is_sitofp() const { return _id == sitofp; }

  protected:
    OpID _id;
};

class RetInst : public Instruction {
  public:
    RetInst(BasicBlock *bb, std::vector<Value *> &&operands);
    std::string print() const final;
};

class BrInst : public Instruction {
  public:
    BrInst(BasicBlock *bb, std::vector<Value *> &&operands);
    std::string print() const final;
};

class BinaryInst : public Instruction {
  public:
    enum BinOp { ADD = 0, SUB, MUL, SDIV, FADD, FSUB, FMUL, FDIV };

    BinaryInst(BasicBlock *bb, std::vector<Value *> &&operands, BinOp op);

    std::string print() const final;

  private:
    static constexpr std::array<OpID, 8> _op_map = {add,  sub,  mul,  sdiv,
                                                    fadd, fsub, fmul, fdiv};
    static constexpr std::array<BinOp, 4> _int_op = {ADD, SUB, MUL, SDIV};
    static constexpr std::array<BinOp, 4> _float_op = {FADD, FSUB, FMUL, FDIV};
    static Type *_deduce_type(BasicBlock *bb, BinOp op);
};

class AllocaInst : public Instruction {
  public:
    AllocaInst(BasicBlock *bb, std::vector<Value *> &&operands,
               Type *elem_type);
    std::string print() const final;
};

class LoadInst : public Instruction {
  public:
    LoadInst(BasicBlock *bb, std::vector<Value *> &&operands);
    std::string print() const final;

  private:
    static Type *_deduce_type(BasicBlock *bb,
                              const std::vector<Value *> &operands);
};

class StoreInst : public Instruction {
  public:
    StoreInst(BasicBlock *bb, std::vector<Value *> &&operands);
    std::string print() const final;
};

class CmpInst : public Instruction {
  public:
    enum CmpOp { EQ, NE, GT, GE, LT, LE, FEQ, FNE, FGT, FGE, FLT, FLE };
    CmpInst(BasicBlock *bb, std::vector<Value *> &&operands, CmpOp cmp_op);
    std::string print() const final;

  private:
    CmpOp _cmp_op;
    static constexpr std::array<CmpOp, 6> _int_op = {EQ, NE, GT, GE, LT, LE};
    static constexpr std::array<CmpOp, 6> _float_op = {FEQ, FNE, FGT,
                                                       FGE, FLT, FLE};
    static OpID _deduce_id(CmpOp cmp_op);
};

class PhiInst : public Instruction {
  public:
    PhiInst(BasicBlock *bb, std::vector<Value *> &&operands);
    std::string print() const final;
};

class CallInst : public Instruction {
  public:
    CallInst(BasicBlock *bb, std::vector<Value *> &&operands);
    std::string print() const final;

private:
    static Type *_deduce_type(BasicBlock *bb, const std::vector<Value *> &operands);
};

class GepInst : public Instruction {
  public:
    GepInst(BasicBlock *bb, std::vector<Value *> &&operands);
    std::string print() const final;
};

class Fp2siInst : public Instruction {
  public:
    Fp2siInst(BasicBlock *bb, std::vector<Value *> &&operands);
    std::string print() const final;
};

class Si2fpInst : public Instruction {
  public:
    Si2fpInst(BasicBlock *bb, std::vector<Value *> &&operands);
    std::string print() const final;
};

} // namespace ir
