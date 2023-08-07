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

#define INST_CLONE(INST)                                                       \
  private:                                                                     \
    INST(BasicBlock *prt, const INST &other)                                   \
        : Instruction(prt, other.get_type(),                                   \
                      {other.operands().begin(), other.operands().end()}) {}   \
                                                                               \
  public:                                                                      \
    Instruction *clone(BasicBlock *prt) const final {                          \
        return new INST{prt, *this};                                           \
    }

#define BIN_INST_CLONE(INST)                                                   \
  private:                                                                     \
    INST(BasicBlock *prt, const INST &other)                                   \
        : Instruction(prt, other.get_type(),                                   \
                      {other.operands().begin(), other.operands().end()}),     \
          _op(other._op) {}                                                    \
                                                                               \
  public:                                                                      \
    Instruction *clone(BasicBlock *prt) const final {                          \
        return new INST{prt, *this};                                           \
    }

#define CMP_INST_CLONE(INST)                                                   \
  private:                                                                     \
    INST(BasicBlock *prt, const INST &other)                                   \
        : Instruction(prt, other.get_type(),                                   \
                      {other.operands().begin(), other.operands().end()}),     \
          _cmp_op(other._cmp_op) {}                                            \
                                                                               \
  public:                                                                      \
    Instruction *clone(BasicBlock *prt) const final {                          \
        return new INST{prt, *this};                                           \
    }

namespace ir {

class Function;
class BasicBlock;
class InstructionVisitor;

class Instruction : public User, public ilist<Instruction>::node {

    // a workaround for move_inst, which requires modify parent bb
    friend BasicBlock;

  public:
    Instruction(BasicBlock *prt, Type *type, std::vector<Value *> &&operands);
    Instruction(const Instruction &) = delete;
    Instruction &operator=(const Instruction &) = delete;

    BasicBlock *get_parent() { return _parent; }

    virtual std::any accept(InstructionVisitor *visitor) const = 0;
    virtual Instruction *clone(BasicBlock *prt) const = 0;

  protected:
    static std::vector<Value *> _mix2vec(Value *first,
                                         const std::vector<Value *> &vec) {
        std::vector<Value *> ret{first};
        ret.insert(ret.end(), vec.begin(), vec.end());
        return ret;
    }

    BasicBlock *_parent;
};

class RetInst : public Instruction {
  public:
    // return a value
    RetInst(BasicBlock *prt, Value *ret_val);
    // void return
    RetInst(BasicBlock *prt);

    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }

    INST_CLONE(RetInst)
};

/* @Constructor: link automatically
 * @Destructor: unlink automatically */
class BrInst : public Instruction {
  public:
    // unconditional jump
    BrInst(BasicBlock *prt, BasicBlock *to);
    // conditional br
    BrInst(BasicBlock *prt, Value *cond, BasicBlock *TBB, BasicBlock *FBB);

    ~BrInst();

    std::string print() const final;

    std::any accept(InstructionVisitor *visitor) const override {
        return visitor->visit(this);
    }

    void set_operand(size_t idx, Value *value,
                     bool modify_op_use = true) override;

    virtual Instruction *clone(BasicBlock *prt) const final {
        if (this->operands().size() == 3) {
            return new BrInst(prt, this->get_operand(0),
                              as_a<BasicBlock>(this->get_operand(1)),
                              as_a<BasicBlock>(this->get_operand(2)));
        } else if (this->operands().size() == 1) {
            return new BrInst(prt, as_a<BasicBlock>(this->get_operand(0)));
        } else {
            throw unreachable_error{};
        }
    }

  private:
    void link();
    void unlink();
};

class IBinaryInst : public Instruction {
  public:
    enum IBinOp { ADD = 0, SUB, MUL, SDIV, SREM, XOR };

    IBinaryInst(BasicBlock *prt, IBinOp op, Value *lhs, Value *rhs);

    std::string print() const final;

    IBinOp get_ibin_op() const { return _op; }
    Value *lhs() const { return get_operand(0); }
    Value *rhs() const { return get_operand(1); }

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }

  private:
    IBinOp _op;

    BIN_INST_CLONE(IBinaryInst)
};

class FBinaryInst : public Instruction {
  public:
    enum FBinOp { FADD, FSUB, FMUL, FDIV };

    FBinaryInst(BasicBlock *prt, FBinOp op, Value *lhs, Value *rhs);

    std::string print() const final;

    FBinOp get_fbin_op() const { return _op; }

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }

  private:
    FBinOp _op;

    BIN_INST_CLONE(FBinaryInst)
};

class AllocaInst : public Instruction {
  public:
    AllocaInst(BasicBlock *prt, Type *elem_type);
    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }

    INST_CLONE(AllocaInst)
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

    INST_CLONE(LoadInst)
};

class StoreInst : public Instruction {
  public:
    StoreInst(BasicBlock *prt, Value *v, Value *ptr);
    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }

    INST_CLONE(StoreInst)
};

class ICmpInst : public Instruction {
  public:
    enum ICmpOp { EQ, NE, GT, GE, LT, LE };
    ICmpInst(BasicBlock *prt, ICmpOp cmp_op, Value *lhs, Value *rhs);
    std::string print() const final;

    static ICmpOp opposite_icmp_op(ICmpOp op);
    static ICmpOp not_icmp_op(ICmpOp op);

    ICmpOp get_icmp_op() const { return _cmp_op; }
    Value *lhs() const { return get_operand(0); }
    Value *rhs() const { return get_operand(1); }

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }

  private:
    ICmpOp _cmp_op;

    CMP_INST_CLONE(ICmpInst)
};

class FCmpInst : public Instruction {
  public:
    enum FCmpOp { FEQ, FNE, FGT, FGE, FLT, FLE };
    FCmpInst(BasicBlock *prt, FCmpOp cmp_op, Value *lhs, Value *rhs);
    std::string print() const final;

    FCmpOp get_fcmp_op() const { return _cmp_op; }

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }

  private:
    FCmpOp _cmp_op;

    CMP_INST_CLONE(FCmpInst)
};

class PhiInst : public Instruction {
  public:
    // @values: the definition list.
    PhiInst(BasicBlock *prt, Value *base);
    PhiInst(BasicBlock *prt, Type *type);

    void add_phi_param(Value *val, BasicBlock *bb);

    using Pair = std::pair<Value *, BasicBlock *>;

    std::vector<Pair> to_pairs() const;
    void from_pairs(const std::vector<Pair> &pairs);

    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }

  private:
    static std::vector<Value *> _get_op(const std::vector<Value *> &values);

    INST_CLONE(PhiInst)
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

    INST_CLONE(CallInst)
};

class Fp2siInst : public Instruction {
  public:
    Fp2siInst(BasicBlock *prt, Value *floatv);
    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }

    INST_CLONE(Fp2siInst)
};

class Si2fpInst : public Instruction {
  public:
    Si2fpInst(BasicBlock *prt, Value *intv);
    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }

    INST_CLONE(Si2fpInst)
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

    INST_CLONE(GetElementPtrInst)
};

class ZextInst : public Instruction {
  public:
    ZextInst(BasicBlock *prt, Value *boolv);
    std::string print() const final;

    virtual std::any accept(InstructionVisitor *visitor) const {
        return visitor->visit(this);
    }

    INST_CLONE(ZextInst)
};

} // namespace ir
