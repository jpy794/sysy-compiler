#include "ast.hh"
#include "module.hh"
#include <memory>

class IRBuilder {
  private:
    std::unique_ptr<ir::Module> _module;

  public:
    IRBuilder(const ast::AST &ast);
    std::unique_ptr<ir::Module> release_module() { return std::move(_module); }
};