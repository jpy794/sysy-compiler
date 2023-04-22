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
        std::string name;
        PtrList<Expr> dims;
    };
    BaseType ret_type;
    std::string fun_name;
    std::vector<Param> params;
    Ptr<BlockStmt> body;
    std::any accept(ASTVisitor &visitor) const override {
        return visitor.visit(*this);
    }
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

    static RawAST parse_sysy_src(const std::string &src);

  private:
    Ptr<Root> root;
};

} // namespace ast