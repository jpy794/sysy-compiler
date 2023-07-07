#pragma once

#include "err.hh"
#include "ilist.hh"
#include "inst_visitor.hh"
#include "type.hh"
#include "user.hh"
#include "utils.hh"
#include "value.hh"

#include <any>
#include <array>
#include <cassert>
#include <optional>
#include <vector>

namespace ir {

class Function;
class BasicBlock;
class InstructionVisitor;

// NOTE: the Instruction class(and dereived) does not care about inserting back
// to parent BB's instruction list.
class Instruction : public User, public ilist<Instruction>::node {

  public:
    Instruction(BasicBlock *prt, Type *type, std::vector<Value *> &&operands);
    Instruction(const Instruction &) = delete;
    Instruction &operator=(const Instruction &) = delete;

    BasicBlock *get_parent() { return _parent; }

    virtual std::any accept(InstructionVisitor *visitor) const = 0;

  protected:
    static std::vector<Value *> _mix2vec(Value *first,
                                         const std::vector<Value *> &vec) {
        std::vector<Value *> ret{first};
        ret.insert(ret.end(), vec.begin(), vec.end());
        return ret;
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

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }
};

class BrInst : public Instruction {
  public:
    // unconditional jump
    BrInst(BasicBlock *prt, BasicBlock *to);
    // conditional br
    BrInst(BasicBlock *prt, Value *cond, BasicBlock *TBB, BasicBlock *FBB);
    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }
};

class IBinaryInst : public Instruction {
  public:
    enum IBinOp { ADD = 0, SUB, MUL, SDIV, SREM, XOR };

    IBinaryInst(BasicBlock *prt, IBinOp op, Value *lhs, Value *rhs);

    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }

  private:
    IBinOp _op;
};

class FBinaryInst : public Instruction {
  public:
    enum FBinOp { FADD, FSUB, FMUL, FDIV, FREM };

    FBinaryInst(BasicBlock *prt, FBinOp op, Value *lhs, Value *rhs);

    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }

  private:
    FBinOp _op;
};

class AllocaInst : public Instruction {
  public:
    AllocaInst(BasicBlock *prt, Type *elem_type);
    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }
};

class LoadInst : public Instruction {
  public:
    LoadInst(BasicBlock *prt, Value *ptr);
    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }

  private:
    static Type *_deduce_type(Value *ptr);
};

class StoreInst : public Instruction {
  public:
    StoreInst(BasicBlock *prt, Value *v, Value *ptr);
    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }
};

class ICmpInst : public Instruction {
  public:
    enum ICmpOp { EQ, NE, GT, GE, LT, LE };
    ICmpInst(BasicBlock *prt, ICmpOp cmp_op, Value *lhs, Value *rhs);
    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }

  private:
    ICmpOp _cmp_op;
};

class FCmpInst : public Instruction {
  public:
    enum FCmpOp { FEQ, FNE, FGT, FGE, FLT, FLE };
    FCmpInst(BasicBlock *prt, FCmpOp cmp_op, Value *lhs, Value *rhs);
    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }

  private:
    FCmpOp _cmp_op;
};

class PhiInst : public Instruction {
  public:
    // @values: the definition list.
    // No bb passed in here, cause the parent bb of can be deduced
    PhiInst(BasicBlock *prt, std::vector<Value *> &&values);

    template <typename... Args>
    PhiInst(BasicBlock *bb, Args &&...args)
        : PhiInst(bb, {static_cast<Value *>(args)...}) {}

    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }

  private:
    static std::vector<Value *> _get_op(const std::vector<Value *> &values);
};

class CallInst : public Instruction {
  public:
    CallInst(BasicBlock *prt, Function *func, std::vector<Value *> &&params);

    template <typename... Args>
    CallInst(BasicBlock *bb, Function *func, Args &&...args)
        : CallInst(bb, func, {static_cast<Value *>(args)...}) {}

    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }

  private:
    static Type *_deduce_type(BasicBlock *prt,
                              const std::vector<Value *> &operands);
};

class Fp2siInst : public Instruction {
  public:
    Fp2siInst(BasicBlock *prt, Value *floatv);
    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }
};

class Si2fpInst : public Instruction {
  public:
    Si2fpInst(BasicBlock *prt, Value *intv);
    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }
};

class GetElementPtrInst : public Instruction {
  public:
    GetElementPtrInst(BasicBlock *prt, Value *baseptr,
                      std::vector<Value *> &&offs);

    template <typename... Args>
    GetElementPtrInst(BasicBlock *bb, Value *baseptr, Args &&...args)
        : GetElementPtrInst(bb, baseptr, {static_cast<Value *>(args)...}) {}

    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }

  private:
    static Type *_deduce_type(BasicBlock *prt, Value *baseptr,
                              const std::vector<Value *> &offs);
};

class ZextInst : public Instruction {
  public:
    ZextInst(BasicBlock *prt, Value *boolv);
    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }
};

} // namespace ir
