#pragma once

#include "user.hh"
#include "ilist.hh"

namespace ir {

class BasicBlock;
class Module;
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
    Instruction(Type *type, const std::string& name, OpID id, unsigned num_ops,
                std::vector<Value *> &operands, BasicBlock *parent);
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
        
    private:
        unsigned _num_ops;
        BasicBlock* _parent;
};

class RetInst:public Instruction{
    public:
        RetInst(Type* type, OpID id, std::vector<Value*>& operands, BasicBlock* parent);
        ~RetInst()=default;
        static Instruction* create(OpID id, std::vector<Value*>&& operands, BasicBlock* parent);
        std::string print() const override;
    private:
};
class BrInst:public Instruction{
    public: 
        BrInst(Type* type, OpID id, std::vector<Value*>& operands, BasicBlock* parent);
        ~BrInst()=default;
        static Instruction* create(OpID id, std::vector<Value*>&& operands, BasicBlock* parent);
        std::string print() const override;
    private:
};
class BinaryInst:public Instruction{
    public: 
        BinaryInst(Type* type, OpID id, std::vector<Value*>& operands, BasicBlock* parent);
        ~BinaryInst()=default;
        static Instruction* create(OpID id, std::vector<Value*>&& operands, BasicBlock* parent);
        std::string print() const override;
        static bool is_int_bina(OpID id){
            return id==add or id==sub or id==mul or id==sdiv;
        }

    static bool is_float_bina(OpID id) {
        return id == fadd or id == fsub or id == fmul or id == fdiv;
    }

  private:
};
class AllocaInst:public Instruction{
    public: 
        AllocaInst(Type* type, OpID id, std::vector<Value*>&& operands, BasicBlock* parent);
        ~AllocaInst()=default;
        static Instruction* create(Type* element_ty, OpID id, BasicBlock *parent);
        std::string print() const override;
    private:
};
class LoadInst:public Instruction{
    public: 
        LoadInst(Type* type, OpID id, std::vector<Value*>& operands, BasicBlock* parent);
        ~LoadInst()=default;
        static Instruction* create(OpID id, std::vector<Value*>&& operands, BasicBlock* parent);
        std::string print() const override;
    private:
};
class StoreInst:public Instruction{
    public: 
        StoreInst(Type* type, OpID id, std::vector<Value*>& operands, BasicBlock* parent);
        ~StoreInst()=default;
        static Instruction* create(OpID id, std::vector<Value*>&& operands, BasicBlock* parent);
        std::string print() const override;
    private:
};
class CmpInst:public Instruction{
    public: 
        enum CmpOp{
          EQ, // ==
          NE, // !=
          GT, // >
          GE, // >=
          LT, // <
          LE  // <=
        };
        CmpInst(Type* type, CmpOp id, std::vector<Value*>& operands, BasicBlock* parent);
        ~CmpInst()=default;
        static Instruction* create(CmpOp id, std::vector<Value*>&& operands, BasicBlock* parent);
        std::string print() const override;
    private:
        CmpOp _id;
};
class FCmpInst:public Instruction{
    public: 
        enum FCmpOp{
          EQ, // ==
          NE, // !=
          GT, // >
          GE, // >=
          LT, // <
          LE  // <=
        };
        FCmpInst(Type* type, FCmpOp id, std::vector<Value*>& operands, BasicBlock* parent);
        ~FCmpInst()=default;
        static Instruction* create(FCmpOp id, std::vector<Value*>&& operands, BasicBlock* parent);
        std::string print() const override;
    private:
        FCmpOp _id;
};
class PhiInst:public Instruction{
    public: 
        PhiInst(Type* type, OpID id, std::vector<Value*>& operands, BasicBlock* parent);
        ~PhiInst()=default;
        static Instruction* create(OpID id, std::vector<Value*>&& operands, BasicBlock* parent);
        std::string print() const override;
    private:
};
class CallInst : public Instruction {
    public: 
        CallInst(Type* type, OpID id, std::vector<Value*>& operands, BasicBlock* parent);
        ~CallInst()=default;
        static Instruction* create(OpID id, std::vector<Value*>&& operands, BasicBlock* parent);
        std::string print() const override;
    private:

  private:
};

class GeteInst : public Instruction {
  public:
    GeteInst(Type *type, OpID id, std::vector<Value *> &operands,
             BasicBlock *parent);
    ~GeteInst() = default;
    static Instruction *create(OpID id, std::vector<Value *> &&operands,
                               BasicBlock *parent);
    std::string print() const override;

  private:
};

class Fp2siInst : public Instruction {
  public:
    Fp2siInst(Type *type, OpID id, std::vector<Value *> &operands,
              BasicBlock *parent);
    ~Fp2siInst() = default;
    static Instruction *create(OpID id, std::vector<Value *> &&operands,
                               BasicBlock *parent);
    std::string print() const override;
  private:
};

class Si2fpInst : public Instruction {
  public:
    Si2fpInst(Type *type, OpID id, std::vector<Value *> &operands,
              BasicBlock *parent);
    ~Si2fpInst() = default;
    static Instruction *create(OpID id, std::vector<Value *> &&operands,
                               BasicBlock *parent);
    std::string print() const override;
  private:
};

} // namespace ir
