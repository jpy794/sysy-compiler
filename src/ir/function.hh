#pragma once

#include "module.hh"
#include "value.hh"
#include <string>

namespace ir {

class BasicBlock;
class Argument;

class Function : public Value, public ilist<Function>::node {
  public:
    static Function *create(FuncType *type, std::string &name, Module *parent);
    // Module
    Module *get_module() const { return _parent; }

    // Type
    Type *get_return_type() const;

    // Arguments
    unsigned get_num_of_args() const { return _args.size(); }

    const ilist<Argument> &get_args();

    void add_arg(Argument *arg);

    // BasicBlock
    unsigned get_num_basic_blcoks() const { return _bbs.size(); }

    BasicBlock &get_entry_block() { return _bbs.front(); }

    void add_basic_block(BasicBlock *bb);

    const ilist<BasicBlock> &get_basic_blocks() { return _bbs; }

    // seq_cnt
    unsigned get_seq() {return _seq_cnt++;}
  private:
    Function(FuncType *type, std::string &name, Module *parent);
    ilist<Argument> _args;
    ilist<BasicBlock> _bbs;
    Module *_parent;
    unsigned _seq_cnt;
};

class Argument : public Value, public ilist<Argument>::node {
  public:
    Argument(Type *type, Function *parent)
        : Value(type, "arg" + std::to_string(parent->get_seq())), _parent(parent) {}
    ~Argument() = default;
    Function* get_function() const { return _parent;}

  private:
    Function *_parent;
};
} // namespace ir