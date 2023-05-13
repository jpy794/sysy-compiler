#include "ast.hh"
#include "instruction.hh"
#include "module.hh"
#include "value.hh"
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

class Scope {
  public:
    void enter() { _stack.push_back({}); }

    void exit() { _stack.pop_back(); }

    void push(std::string name, ir::Value *val) {
        if (_stack.back().find(name) != _stack.back().end())
            _stack.back()[name] = val;
        else
            throw std::logic_error{"the name of " + name + " has been defined"};
    }

    ir::Value *find(std::string name) {
        if (_stack.back().find(name) != _stack.back().end())
            return _stack.back()[name];
        else
            return nullptr;
    }

    bool is_in_global() { return _stack.size() == 1; }

  private:
    std::vector<std::unordered_map<std::string, ir::Value *>> _stack;
};
class SysyBuilder : public ast::ASTVisitor {
  public:
    SysyBuilder() {
        _m = std::unique_ptr<ir::Module>(new ir::Module("Sysy Module"));
    }

  private:
    Scope scope;
    std::unique_ptr<ir::Module> _m;

  private:
    /* visit for super class pointer */
    using ast::ASTVisitor::visit;
    /* do NOT save pointer to AST tree node in visitor */
    std::any visit(const ast::Root &node) override final;
    /* global */
    std::any visit(const ast::FunDefGlobal &node) override final;
    std::any visit(const ast::VarDefGlobal &node) override final;
    /* stmt */
    std::any visit(const ast::BlockStmt &node) override final;
    std::any visit(const ast::IfStmt &node) override final;
    std::any visit(const ast::WhileStmt &node) override final;
    std::any visit(const ast::BreakStmt &node) override final;
    std::any visit(const ast::ContinueStmt &node) override final;
    std::any visit(const ast::ReturnStmt &node) override final;
    std::any visit(const ast::AssignStmt &node) override final;
    std::any visit(const ast::VarDefStmt &node) override final;
    std::any visit(const ast::ExprStmt &node) override final;
    /* expr */
    std::any visit(const ast::CallExpr &node) override final;
    std::any visit(const ast::LiteralExpr &node) override final;
    std::any visit(const ast::LValExpr &node) override final;
    std::any visit(const ast::BinaryExpr &node) override final;
    std::any visit(const ast::UnaryExpr &node) override final;
    /* raw node should only exisits in raw_ast
       ast visitor should throw exception on following nodes */
    std::any visit(const ast::RawVarDefStmt &node) override final;
    std::any visit(const ast::RawFunDefGlobal &node) override final;
    std::any visit(const ast::RawVarDefGlobal &node) override final;
};