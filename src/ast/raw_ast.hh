#pragma once

#include "ast.hh"

namespace ast {

struct RawInitExpr : Expr {
    PtrList<Expr> init_vals;
};

struct RawVarDefStmt : Stmt {
    struct Entry {
        bool is_const;
        BaseType type;
        std::string var_name;
        PtrList<Expr> dims;
        Ptr<Expr> init_vals;
    };
    PtrList<Entry> var_defs;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
};

struct RawFunDefGlobal : Global {
    struct Param {
        BaseType type;
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

class RawASTVisitor : public ASTVisitor {
  public:
    using ASTVisitor::visit;
    virtual std::any visit(const RawInitExpr &node) = 0;
    virtual std::any visit(const RawVarDefStmt &node) = 0;
    virtual std::any visit(const RawFunDefGlobal &node) = 0;
};

class RawAST {
  public:
    void visit(ASTVisitor &visitor) {
        if (not root) {
            throw std::logic_error{"trying to visit an empty raw AST"};
        }
        /* it's safe to strip unique_ptr here
           as long as visitor does not save ptr to AST node */
        visitor.visit(*root.get());
    }

    // parse a sysy source file
    RawAST(const std::string &src);

    // delete copy constructor
    RawAST(const RawAST &rhs) = delete;
    RawAST &operator=(const RawAST &rhs) = delete;

  private:
    Ptr<Root> root;
};

} // namespace ast