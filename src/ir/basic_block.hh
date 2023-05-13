#pragma once

#include "err.hh"
#include "ilist.hh"
#include "instruction.hh"
#include "type.hh"
#include "utils.hh"
#include <cassert>
#include <type_traits>

namespace ir {

class Function;

class BasicBlock : public Value, public ilist<BasicBlock>::node {
  public:
    BasicBlock(Function *func);

    template <typename Inst, typename... Args>
    Inst *create_inst(Args &&...args) {
        // check for RetInst
        if constexpr (std::is_same<RetInst, Inst>::value) {
            assert(not is_terminated());
        }
        // check for BrInst
        if constexpr (std::is_same<BrInst, Inst>::value) {
            assert(not is_terminated());
            _link(std::forward<Args>(args)...);
        }

        _insts.push_back(new Inst{this, std::forward<Args>(args)...});
        return as_a<Inst>(&_insts.back());
    }

    std::vector<BasicBlock *> &pre_bbs() { return _pre_bbs; }
    std::vector<BasicBlock *> &suc_bbs() { return _suc_bbs; }
    ilist<Instruction> &insts() { return _insts; }

    Function *get_func() const { return _func; }
    const std::vector<BasicBlock *> &get_pre_bbs() const { return _pre_bbs; }
    const std::vector<BasicBlock *> &get_suc_bbs() const { return _pre_bbs; }
    const ilist<Instruction> &get_insts() const { return _insts; }

    bool is_terminated() const {
        return _insts.size() != 0 and (is_a<const BrInst>(&_insts.back()) or
                                       is_a<const RetInst>(&_insts.back()));
    }

    std::string print() const final;

  private:
    std::vector<BasicBlock *> _pre_bbs;
    std::vector<BasicBlock *> _suc_bbs;
    ilist<Instruction> _insts;
    Function *const _func;

    // possible_to maybe bool or BasicBlock
    template <typename boba, typename... BBs>
    void _link(boba *possible_to, BBs... rest) {
        if constexpr (std::is_base_of<Instruction, boba>::value) {
            assert(is_a<BoolType>(possible_to->get_type()));
        } else if constexpr (std::is_same<BasicBlock, boba>::value) {
            possible_to->_pre_bbs.push_back(this);
            this->_suc_bbs.push_back(possible_to);
        } else {
            throw unreachable_error{"wrong type"};
        }
        _link(std::forward<BBs>(rest)...);
    }
    // function to maintain pre_bbs/suc_bbs
    void _link() {}
};

} // namespace ir
