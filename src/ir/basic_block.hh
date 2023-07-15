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
        // whenever append inst to bb, bb should not be terminated
        assert(not is_terminated());

        // check for BrInst
        if constexpr (std::is_same<BrInst, Inst>::value) {
            _link(std::forward<Args>(args)...);
        }

        _insts.push_back(new Inst{this, std::forward<Args>(args)...});
        return as_a<Inst>(&_insts.back());
    }

    template <typename Inst, typename... Args>
    Inst *insert_inst(const ilist<Instruction>::iterator &it, Args &&...args) {
        // check for RetInst
        if constexpr (std::is_same<RetInst, Inst>::value) {
            assert(not is_terminated());
            assert(it != _insts.end());
        }
        // check for BrInst
        if constexpr (std::is_same<BrInst, Inst>::value) {
            assert(not is_terminated());
            assert(it != _insts.end());
            _link(std::forward<Args>(args)...);
        }
        auto inst = new Inst{this, std::forward<Args>(args)...};
        _insts.insert(it, inst);
        return inst;
    }

    // clone a inst, be careful when cloning from another function as this could
    // invalidate all operands
    Instruction *clone_inst(const ilist<Instruction>::iterator &it,
                            Instruction *other) {
        if (other->is<BrInst>() || other->is<RetInst>()) {
            assert(not is_terminated() && it == _insts.end());
        }
        auto inst = other->clone(this);
        _insts.insert(it, inst);
        return inst;
    }

    // move inst within the same function
    void move_inst(const ilist<Instruction>::iterator &it, Instruction *other) {
        auto other_bb = other->get_parent();
        assert(other_bb->get_func() == _func);
        if (other->is<BrInst>() || other->is<RetInst>()) {
            assert(not is_terminated() && it == _insts.end());
        }
        auto inst = other_bb->insts().release(other);
        inst->_parent = this;
        _insts.insert(it, inst);
    }

    std::vector<BasicBlock *> &pre_bbs() { return _pre_bbs; }
    std::vector<BasicBlock *> &suc_bbs() { return _suc_bbs; }
    ilist<Instruction> &insts() { return _insts; }

    Function *get_func() const { return _func; }
    const std::vector<BasicBlock *> &get_pre_bbs() const { return _pre_bbs; }
    const std::vector<BasicBlock *> &get_suc_bbs() const { return _suc_bbs; }
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
