#pragma once

#include <list>

#include "instruction.hh"
#include "module.hh"

namespace ir {

class Function;
class BasicBlock : public Value, public ilist<BasicBlock>::node {
  public:
    static BasicBlock *create(Function *parent);

    // Function
    Function *get_function() const { return _parent; }

    // BasicBlock
    const std::list<BasicBlock*> &get_pre_basic_blocks() const { return _pre_bbs; }

    const std::list<BasicBlock*> &get_suc_basic_blocks() const { return _suc_bbs; }

    void add_pre_basic_block(BasicBlock *bb) { _pre_bbs.push_back(bb); }

    void add_suc_basic_block(BasicBlock *bb) { _suc_bbs.push_back(bb); }

    unsigned get_num_pre_bbs() const { return _pre_bbs.size(); }

    unsigned get_num_suc_bbs() const { return _suc_bbs.size(); }
    // Instruction
    void add_instruction(Instruction *instr);

    unsigned get_num_of_instr() const { return _instr_list.size(); }

    // print
    std::string print() const override;
  private:
    BasicBlock(Function *parent);
    std::list<BasicBlock*> _pre_bbs;
    std::list<BasicBlock*> _suc_bbs;
    ilist<Instruction> _instr_list;
    Function *_parent;
};

} // namespace ir