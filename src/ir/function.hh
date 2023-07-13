#pragma once

#include "basic_block.hh"
#include "ilist.hh"
#include "type.hh"
#include "value.hh"

#include <cassert>
#include <memory>
#include <string>

namespace ir {

class Argument;
class Module;

class Function : public Value, public ilist<Function>::node {
  public:
    Function(FuncType *type, std::string &&name, bool external = false);
    ~Function();

    // creaters
    template <typename... Args> BasicBlock *create_bb(Args &&...args) {
        _bbs.emplace_back(this, args...);
        return &_bbs.back();
    }

    // getters
    Type *get_return_type() const {
        return dynamic_cast<FuncType *>(get_type())->get_result_type();
    };

    ilist<BasicBlock> &bbs() { return _bbs; }

    const ilist<BasicBlock> &get_bbs() const { return _bbs; }
    const std::vector<Argument *> &get_args() const { return _args; }
    BasicBlock *get_entry_bb() {
        assert(_bbs.size() >= 1);
        return &_bbs.front();
    }
    BasicBlock *get_exit_bb() {
        assert(_bbs.size() >= 2);
        return &*++_bbs.begin();
    }

    // for inst name %op123
    size_t get_inst_seq() { return _inst_seq++; }

    std::string print() const final;

    // external symbol
    bool is_external;

  private:
    std::vector<Argument *> _args;
    ilist<BasicBlock> _bbs;
    size_t _inst_seq;
};

class Argument : public Value, public ilist<Argument>::node {
  public:
    Argument(Function *func, Type *type)
        : Value(type, "%arg" + std::to_string(func->get_inst_seq())),
          _func(func) {}
    Function *get_function() const { return _func; }
    std::string print() const final;

  private:
    Function *const _func;
};

} // namespace ir
