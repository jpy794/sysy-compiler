#pragma once

#include <list>

#include "ilist.hh"
#include "instruction.hh"
#include "type.hh"

namespace ir {
class Module;
class Function;
class BasicBlock : public Value, public ilist<BasicBlock>::node {
  public:
    static BasicBlock *create(Function *parent);

    // Function
    Function *get_function() const { return _parent; }

    // BasicBlock
    const std::vector<BasicBlock*> &get_pre_basic_blocks() const { return _pre_bbs; }

    const std::vector<BasicBlock*> &get_suc_basic_blocks() const { return _suc_bbs; }

    std::vector<BasicBlock*> &get_pre_basic_blocks() { return _pre_bbs; }

    std::vector<BasicBlock*> &get_suc_basic_blocks() { return _suc_bbs; }

    // Instruction

    const ilist<Instruction>& get_instructions() const { return _instr_list; }

    ilist<Instruction>& get_instructions() { return _instr_list; }
    // print
    std::string print() const override;
  private:
    BasicBlock(Function *parent);
    std::vector<BasicBlock*> _pre_bbs;
    std::vector<BasicBlock*> _suc_bbs;
    ilist<Instruction> _instr_list;
    Function *_parent;
};

} // namespace ir