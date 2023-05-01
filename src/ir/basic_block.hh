#pragma once

#include "instruction.hh"
#include "module.hh"

namespace ir {

class Function;
class BasicBlock : public Value, ilist<BasicBlock>::node {
  public:
    static BasicBlock *create(const std::string &name, Function *parent);

    // Function
    Function *get_function() const { return _parent; }

    // BasicBlock
    const ilist<BasicBlock> &get_pre_basic_blocks() { return _pre_bbs; }

    const ilist<BasicBlock> &get_suc_basic_blocks() { return _suc_bbs; }

    void add_pre_basic_block(BasicBlock *bb) { _pre_bbs.push_back(bb); }

    void add_suc_basic_block(BasicBlock *bb) { _suc_bbs.push_back(bb); }

    unsigned get_num_pre_bbs() const { return _pre_bbs.size(); }

    unsigned get_num_suc_bbs() const { return _suc_bbs.size(); }
    // Instruction
    void add_instruction(Instruction *instr);

    unsigned get_num_of_instr() const { return _instr_list.size(); }

  private:
    BasicBlock(const std::string &name, Function *parent);
    ilist<BasicBlock> _pre_bbs;
    ilist<BasicBlock> _suc_bbs;
    ilist<Instruction> _instr_list;
    Function *_parent;
};

} // namespace ir