#pragma once

#include "ilist.hh"
#include "type.hh"
#include "user.hh"
#include "utils.hh"
#include "value.hh"

#include <array>
#include <cassert>
#include <optional>
#include <vector>

namespace ir {

class Function;
class BasicBlock;

class RetInst;

// NOTE: the Instruction class(and dereived) does not care about inserting back
// to parent BB's instruction list.
class Instruction : public User, public ilist<Instruction>::node {
    // TODO: delete opid, we don't need it any more
    enum OpID {
        // Terminator Instructions
        ret,
        br,
        // Standard binary operators
        add,
        sub,
        mul,
        sdiv,
        srem,
        // float binary operators
        fadd,
        fsub,
        fmul,
        fdiv,
        frem,
        // Logic binary operators TODO
        logic_and,
        logic_or,
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
        sitofp,
        zext
    };

  public:
    Instruction(BasicBlock *prt, Type *type, std::vector<Value *> &&operands);
    Instruction(const Instruction &) = delete;
    Instruction &operator=(const Instruction &) = delete;

    virtual ~Instruction() = 0;

    BasicBlock *get_parent() { return _parent; }

    virtual std::string print() const override { return {}; };

  protected:
    static std::vector<Value *> _mix2vec(Value *first,
                                         const std::vector<Value *> &vec) {
        std::vector<Value *> ret{first};
        ret.insert(ret.end(), vec.begin(), vec.end());
        return ret;
    }
    template <typename Array, typename Element>
    static inline bool arrcontains(Array &array, const Element &elem) {
        return std::find(std::begin(array), std::end(array), elem) !=
               std::end(array);
    }

  private:
    BasicBlock *_parent;
};

class RetInst : public Instruction {
  public:
    // return a value
    RetInst(BasicBlock *prt, Value *ret_val);
    // void return
    RetInst(BasicBlock *prt) : Instruction(prt, Types::get().void_type(), {}) {}

    std::string print() const final;
};

class BrInst : public Instruction {
  public:
    // unconditional jump
    BrInst(BasicBlock *prt, BasicBlock *to);
    // conditional br
    BrInst(BasicBlock *prt, Value *cond, BasicBlock *TBB, BasicBlock *FBB);
    std::string print() const final;
};

class BinaryInst : public Instruction {
  public:
    enum BinOp { ADD = 0, SUB, MUL, SDIV, SREM, FADD, FSUB, FMUL, FDIV, FREM };

    BinaryInst(BasicBlock *prt, BinOp op, Value *lhs, Value *rhs);

    std::string print() const final;

  private:
    BinOp _op;
    static constexpr std::array<BinOp, 5> _int_op = {ADD, SUB, MUL, SDIV, SREM};
    static constexpr std::array<BinOp, 5> _float_op = {FADD, FSUB, FMUL, FDIV,
                                                       FREM};
};

class AllocaInst : public Instruction {
  public:
    AllocaInst(BasicBlock *prt, Type *elem_type);
    std::string print() const final;
};

class LoadInst : public Instruction {
  public:
    LoadInst(BasicBlock *prt, Value *ptr);
    std::string print() const final;

  private:
    static Type *_deduce_type(Value *ptr);
};

class StoreInst : public Instruction {
  public:
    StoreInst(BasicBlock *prt, Value *v, Value *ptr);
    std::string print() const final;
};

class CmpInst : public Instruction {
  public:
    enum CmpOp { EQ, NE, GT, GE, LT, LE, FEQ, FNE, FGT, FGE, FLT, FLE };
    CmpInst(BasicBlock *prt, CmpOp cmp_op, Value *lhs, Value *rhs);
    std::string print() const final;

  private:
    CmpOp _cmp_op;
    static constexpr std::array<CmpOp, 6> _int_op = {EQ, NE, GT, GE, LT, LE};
    static constexpr std::array<CmpOp, 6> _float_op = {FEQ, FNE, FGT,
                                                       FGE, FLT, FLE};
};

class PhiInst : public Instruction {
  public:
    // @values: the definition list.
    // No bb passed in here, cause the parent bb of can be deduced
    PhiInst(BasicBlock *prt, std::vector<Value *> &&values);
    std::string print() const final;

  private:
    static std::vector<Value *> _get_op(const std::vector<Value *> &values);
};

class CallInst : public Instruction {
  public:
    CallInst(BasicBlock *prt, Function *func, std::vector<Value *> &&params);
    std::string print() const final;

  private:
    static Type *_deduce_type(BasicBlock *prt,
                              const std::vector<Value *> &operands);
};

class Fp2siInst : public Instruction {
  public:
    Fp2siInst(BasicBlock *prt, Value *floatv);
    std::string print() const final;
};

class Si2fpInst : public Instruction {
  public:
    Si2fpInst(BasicBlock *prt, Value *intv);
    std::string print() const final;
};

class GetElementPtrInst : public Instruction {
  public:
    GetElementPtrInst(BasicBlock *prt, Value *baseptr,
                      std::vector<Value *> &&offs);
    std::string print() const final;

  private:
    static Type *_deduce_type(BasicBlock *prt, Value *baseptr,
                              const std::vector<Value *> &offs);
};
class ZextInst : public Instruction {
  public:
    ZextInst(BasicBlock *prt, Value *boolv);
    std::string print() const final;
};

} // namespace ir
