#pragma once

#include <any>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace ast {

template <typename T> using Ptr = std::unique_ptr<T>;
template <typename T> using PtrList = std::vector<Ptr<T>>;

/* type alias can not be recursive, define a struct template instead */
template <typename T> struct InitVals {
    std::vector<std::variant<T, std::unique_ptr<InitVals>>> literal_or_initvals;
    InitVals() : literal_or_initvals(std::vector<T>{}) {}
    InitVals(std::vector<T> &&literal) : literal_or_initvals(literal) {}
    InitVals(std::vector<std::unique_ptr<InitVals>> &&initvals)
        : literal_or_initvals(initvals) {}
};

enum class BaseType { VOID, FLOAT, INT };
struct SysYType {
    BaseType base;
    /* dim[0] = 0 means that this is a pointer,
       which is only allowed in function parameter */
    std::vector<size_t> dims;
    bool is_ptr() const { return dims.size() > 0 && dims[0] == 0; }
    static SysYType get_type(BaseType base, std::vector<size_t> &&dims) {
        return {base, dims};
    }
};

/* NOP: interpret the cond as it is */
enum class CondOp { LT, GT, LE, GE, EQ, NE, /* unary op */ NOT, NOP };
enum class ExpOp { ADD, SUB, MUL, DIV, MOD, /* unary op */ PLUS, MINUS };

class ASTVisitor;
struct ASTNode {
    virtual ~ASTNode() = default;
    virtual std::any accept(ASTVisitor &visitor) const {
        /* visit phantom node will trigger this exception
           e.g. ASTNode GlobalDefNode StmtNode StmtOrVarDefNode CondOrExpNode
           in case any pure virtual method call */
        throw std::runtime_error{"unexpected phantom ASTNode visit."};
    };
};

/* AST node structure */
struct RootNode;

/* global def */
struct GlobalDefNode;
struct FuncDefNode;

/* stmt or def */
struct StmtOrVarDefNode;
struct StmtNode;
struct BlockNode;
struct IfNode;
struct BreakNode;
struct WhileNode;
struct ContinueNode;
struct ReturnNode;
struct AssignNode;
struct VarDefNode; // also global def

/* cond or exp */
struct CondOrExpNode;
struct CondNode;
struct ExpNode; // also stmt
struct ArithExpNode;
struct CallNode;
struct LiteralNode;
struct LValNode;

/* visitor pattern allows multiple nodes of the same super class
   to be visited through super class pointer (by the virtual accept).
   we can use dynamic_cast instead, but it involves in a lot of if else
   when there're many child classes.
   the drawback of visitor pattern is that we can not change the visitor
   interface to use another return type.
   std::any is useful because it allows us to change return type to
   any (usually the same for derived classes of the same super class),
   no need to modify visitor interface. */

class ASTVisitor {
  public:
    /* visit for super class pointer */
    std::any visit(const ASTNode &node) { return node.accept(*this); }
    /* do NOT save pointer to AST tree node in visitor */
    virtual std::any visit(const RootNode &node) = 0;
    virtual std::any visit(const FuncDefNode &node) = 0;
    virtual std::any visit(const BlockNode &node) = 0;
    virtual std::any visit(const CondNode &node) = 0;
    virtual std::any visit(const IfNode &node) = 0;
    virtual std::any visit(const WhileNode &node) = 0;
    virtual std::any visit(const BreakNode &node) = 0;
    virtual std::any visit(const ContinueNode &node) = 0;
    virtual std::any visit(const ReturnNode &node) = 0;
    virtual std::any visit(const AssignNode &node) = 0;
    virtual std::any visit(const VarDefNode &node) = 0;
    virtual std::any visit(const CallNode &node) = 0;
    virtual std::any visit(const LiteralNode &node) = 0;
    virtual std::any visit(const LValNode &node) = 0;
    virtual std::any visit(const ArithExpNode &node) = 0;
};

struct RootNode : ASTNode {
    PtrList<GlobalDefNode> global_defs;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct GlobalDefNode : ASTNode {};

struct FuncDefNode : GlobalDefNode {
    /* void, int or float */
    BaseType ret_type;
    std::string name;
    std::vector<SysYType> args;
    Ptr<BlockNode> body;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct StmtOrVarDefNode : ASTNode {};
struct StmtNode : StmtOrVarDefNode {};

struct BlockNode : StmtNode {
    PtrList<StmtOrVarDefNode> stmt_defs;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct CondOrExpNode : ASTNode {};

struct CondNode : CondOrExpNode {
    CondOp op;
    /* for unary op, rhs is the only child
       lhs, rhs can be exp or cond node */
    Ptr<CondOrExpNode> lhs, rhs;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct ExpNode : StmtNode, CondOrExpNode {};

struct ArithExpNode : ExpNode {
    ExpOp op;
    /* for unary op, rhs is the only child */
    Ptr<ExpNode> lhs, rhs;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct IfNode : StmtNode {
    Ptr<CondNode> cond;
    Ptr<StmtNode> then_body, else_body;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct WhileNode : StmtNode {
    Ptr<CondNode> cond;
    Ptr<StmtNode> body;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct BreakNode : StmtNode {
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct ContinueNode : StmtNode {
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct ReturnNode : StmtNode {
    Ptr<ExpNode> ret_val;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct AssignNode : StmtNode {
    Ptr<LValNode> lval;
    Ptr<ExpNode> exp;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct LValNode : ExpNode {
    std::string var_name;
    std::vector<size_t> dims;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct LiteralNode : ExpNode {
    BaseType type;
    std::variant<float, int> val;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct CallNode : ExpNode {
    std::string func_name;
    PtrList<ExpNode> exps;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct VarDefNode : GlobalDefNode, StmtOrVarDefNode {

    bool is_const;
    std::string var_name;
    SysYType type;
    std::variant<InitVals<float>, InitVals<int>> init_vals;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

class AST {
  public:
    AST(const std::string &src_file);
    void visit(ASTVisitor &visitor) {
        /* it's safe to strip unique_ptr here
           as long as visitor does not save ptr to AST node */
        visitor.visit(*root.get());
    }

  private:
    Ptr<RootNode> root;
};
} // namespace ast