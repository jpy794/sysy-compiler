#pragma once

#include <any>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace ast {

template <typename T> using Ptr = std::unique_ptr<T>;
template <typename T> using PtrList = std::vector<Ptr<T>>;

enum class BaseType { VOID, FLOAT, INT };

enum class BinOp { ADD, SUB, MUL, DIV, MOD, LT, GT, LE, GE, EQ, NE, AND, OR };
enum class UnaryOp { PlUS, MINUS, /* only apply to cond expr */ NOT };

class ASTVisitor;
struct ASTNode {
    virtual ~ASTNode() = default;
    virtual std::any accept(ASTVisitor &visitor) const {
        /* visiting a phantom node will trigger this exception
           e.g. Expr Stmt Global
           in case any pure virtual method call */
        throw std::runtime_error{"unexpected phantom ASTNode visit."};
    };
};

/* AST node structure */
struct Root;

/* global */
struct Global : ASTNode {};
struct FunDefGlobal;
struct VarDefGlobal;

/* stmt: return ir or bb */
struct Stmt : ASTNode {};
struct BlockStmt;
struct IfStmt;
struct BreakStmt;
struct WhileStmt;
struct ContinueStmt;
struct ReturnStmt;
struct AssignStmt;
struct VarDefStmt;
struct ExprStmt;

/* expr: return a variable and ir that generates it */
struct Expr : ASTNode {};
struct UnaryExpr;
struct BinaryExpr;
struct CallExpr;
struct LiteralExpr;
struct LValExpr;

/* raw node that should only exisit in raw_ast */
struct RawVarDefStmt;
struct RawFunDefGlobal;
struct RawVarDefGlobal;

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
    virtual std::any visit(const Root &node) = 0;
    /* global */
    virtual std::any visit(const FunDefGlobal &node) = 0;
    virtual std::any visit(const VarDefGlobal &node) = 0;
    /* stmt */
    virtual std::any visit(const BlockStmt &node) = 0;
    virtual std::any visit(const IfStmt &node) = 0;
    virtual std::any visit(const WhileStmt &node) = 0;
    virtual std::any visit(const BreakStmt &node) = 0;
    virtual std::any visit(const ContinueStmt &node) = 0;
    virtual std::any visit(const ReturnStmt &node) = 0;
    virtual std::any visit(const AssignStmt &node) = 0;
    virtual std::any visit(const VarDefStmt &node) = 0;
    virtual std::any visit(const ExprStmt &node) = 0;
    /* expr */
    virtual std::any visit(const CallExpr &node) = 0;
    virtual std::any visit(const LiteralExpr &node) = 0;
    virtual std::any visit(const LValExpr &node) = 0;
    virtual std::any visit(const BinaryExpr &node) = 0;
    virtual std::any visit(const UnaryExpr &node) = 0;
};

struct Root : ASTNode {
    PtrList<Global> globals;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct FunDefGlobal : Global {
    struct Param {
        BaseType type;
        std::string name;
        /* evaluated when building AST
           dim[0] should always be 0 */
        std::vector<size_t> dims;
    };
    /* void, int or float */
    BaseType ret_type;
    std::string fun_name;
    std::vector<Param> params;
    Ptr<BlockStmt> body;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct VarDefGlobal : Global {
    Ptr<VarDefStmt> vardef_stmt;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct UnaryExpr : Expr {
    UnaryOp op;
    Ptr<Expr> rhs;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct BinaryExpr : Expr {
    BinOp op;
    Ptr<Expr> lhs, rhs;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct LValExpr : Expr {
    std::string var_name;
    PtrList<Expr> idxs;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct LiteralExpr : Expr {
    BaseType type;
    std::variant<float, int> val;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct CallExpr : Expr {
    std::string fun_name;
    PtrList<Expr> args;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct IfStmt : Stmt {
    Ptr<Expr> cond;
    Ptr<Stmt> then_body;
    std::optional<Ptr<Stmt>> else_body;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct WhileStmt : Stmt {
    Ptr<Expr> cond;
    Ptr<Stmt> body;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct BreakStmt : Stmt {
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct ContinueStmt : Stmt {
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct ReturnStmt : Stmt {
    std::optional<Ptr<Expr>> ret_val;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct AssignStmt : Stmt {
    std::string var_name;
    PtrList<Expr> idxs;
    Ptr<Expr> val;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct VarDefStmt : Stmt {
    /*     struct initPair {
     *         size_t pos;
     *         Ptr<Expr> ptr;
     *
     *         explicit initPair(size_t _pos, Ptr<Expr> &&_ptr)
     *             : pos(_pos), ptr(std::move(_ptr)) {}
     *
     *     }; */
    bool is_const;
    BaseType type;
    std::string var_name;
    /* all dims should be non-zero
       evaluated when building the AST */
    std::vector<size_t> dims;
    /* @init_vals: info of designated initializer
     *
     * Has the following features:
     * - flattened initialize-list
     * - each kv-pair in the map is the designated initialize-value
     * - for const and gloabl, the inner node is LiteralExpr actually
     * - for other cases, no guarantee for epxr
     *
     * Semantic:
     * - nullopt indicates no initialization. global variable always has init
     *   due to the rule of implicit 0-init
     * - for the BaseType var init case, init_vals always holds `single`
     *   kv-pair: <0, value>
     * - for inited array type(not nullopt), if init_vals contains target idx,
     *   use the mapped value, or means implicit 0 init.
     *
     */
    std::optional<std::map<size_t, Ptr<Expr>>> init_vals;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct BlockStmt : Stmt {
    PtrList<Stmt> stmts;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct ExprStmt : Stmt {
    /* nullopt means it is an empty stmt, aka a semicolon */
    std::optional<Ptr<Expr>> expr;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

/* raw node that should only exisit in raw_ast */

struct RawVarDefGlobal : Global {
    Ptr<RawVarDefStmt> vardef_stmt;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct RawVarDefStmt : Stmt {
    struct InitList {
        bool is_zero_list;
        std::variant<Ptr<Expr>, PtrList<InitList>> val;
    };
    struct Entry {
        bool is_const;
        BaseType type;
        std::string var_name;
        PtrList<Expr> dims;
        /* nullopt means there's no initval for the entry */
        std::optional<Ptr<InitList>> init_list;
    };
    PtrList<Entry> var_defs;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct RawFunDefGlobal : Global {
    struct Param {
        BaseType type;
        std::string name;
        bool is_ptr;
        // dim starts from the 2nd one
        PtrList<Expr> dims;
    };
    BaseType ret_type;
    std::string fun_name;
    PtrList<Param> params;
    Ptr<BlockStmt> body;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

class RawAST;

class AST {
  public:
    AST(RawAST &&raw_ast);
    std::any accept(ASTVisitor &visitor) const {
        if (not root) {
            throw std::logic_error{"trying to visit an empty AST"};
        }
        /* it's safe to strip unique_ptr here
           as long as visitor does not save ptr to AST node */
        return visitor.visit(*root.get());
    }

  private:
    Ptr<Root> root;
};

std::ostream &operator<<(std::ostream &os, const AST &ast);

} // namespace ast
