#pragma once

#include "ilist.hh"
#include "instruction.hh"
#include "type.hh"

namespace ir {

class Function;

class BasicBlock : public Value, public ilist<BasicBlock>::node {
  public:
    BasicBlock(Function *func);

    template <typename Inst, typename... Args>
    Inst *create_inst(Args &&...args) {
        _insts.push_back(new Inst{this, std::forward<Args>(args)...});
        return dynamic_cast<Inst *>(&_insts.back());
    }

    std::vector<BasicBlock *> &pre_bbs() { return _pre_bbs; }
    std::vector<BasicBlock *> &suc_bbs() { return _suc_bbs; }
    ilist<Instruction> &insts() { return _insts; }

    Function *get_func() const { return _func; }
    const std::vector<BasicBlock *> &get_pre_bbs() const { return _pre_bbs; }
    const std::vector<BasicBlock *> &get_suc_bbs() const { return _pre_bbs; }
    const ilist<Instruction> &get_insts() const { return _insts; }

    std::string print() const final;

  private:
    std::vector<BasicBlock *> _pre_bbs;
    std::vector<BasicBlock *> _suc_bbs;
    ilist<Instruction> _insts;
    Function *const _func;
};

} // namespace ir
