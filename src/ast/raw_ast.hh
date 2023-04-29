#pragma once

#include "ast.hh"

namespace ast {

class RawAST {
  public:
    std::any visit(ASTVisitor &visitor) {
        if (not root) {
            throw std::logic_error{"trying to visit an empty raw AST"};
        }
        /* it's safe to strip unique_ptr here
           as long as visitor does not save ptr to AST node */
        return visitor.visit(*root.get());
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