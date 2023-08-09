#pragma once

#include "err.hh"
#include "ilist.hh"
#include "instruction.hh"
#include "type.hh"
#include "utils.hh"
#include <cassert>
#include <stdexcept>
#include <type_traits>

namespace ir {

class Function;

/* - prev/succ links: maintain by BrInst automatically
 * - use-chain: User's constructor and destructor will take care it
 * need take care of:
 * - !terminated: ret/br will check not termination, but other insts donot, so i
 * - !insert position: the ret/br should be inserted to end only
 */
class BasicBlock : public Value, public ilist<BasicBlock>::node {
    friend class BrInst;

    using InstIter = ilist<Instruction>::iterator;

  public:
    BasicBlock(Function *func);

    template <typename Inst, typename... Args>
    Inst *create_inst(Args &&...args) {
        assert(not is_terminated());
        _insts.push_back(new Inst{this, std::forward<Args>(args)...});
        return as_a<Inst>(&_insts.back());
    }

    template <typename Inst, typename... Args>
    Inst *insert_inst(const InstIter &it, Args &&...args) {
        avoid_push_back_when_terminated(it);
        // check for RetInst and BrInst
        if (std::is_same<RetInst, Inst>::value or
            std::is_same<BrInst, Inst>::value) {
            assert(it == _insts.end());
        }
        auto inst = new Inst{this, std::forward<Args>(args)...};
        _insts.insert(it, inst);
        return inst;
    }

    // clone inst within the same function
    Instruction *clone_inst(const InstIter &it, Instruction *other,
                            bool diff_func = false);

    /* // @deprecated
     * // clone only for the private member except for operands which should be
     * // replaced by caller
     * Instruction *clone_inst_skeleton(const InstIter &it, Instruction *other);
     */

    // TODO make return void cause move_inst is ptr invariant
    // move inst within the same function
    Instruction *move_inst(const InstIter &it, Instruction *other);
    // erase inst in this block
    // you should handle inst's occur at other places
    InstIter erase_inst(const InstIter &it);

    bool is_terminated() const;
    std::string print() const final;

    Function *get_func() const { return _func; }
    ilist<Instruction> &insts() { return _insts; }
    /* @deprecated: donot return variable vectors anymore!
     * std::set<BasicBlock*> &pre_bbs() { return _pre_bbs; }
     * std::set<BasicBlock*> &suc_bbs() { return _suc_bbs; } */
    const std::set<BasicBlock *> &pre_bbs() const { return _pre_bbs; }
    const std::set<BasicBlock *> &suc_bbs() const { return _suc_bbs; }
    const ilist<Instruction> &insts() const { return _insts; }

    BrInst &br_inst() {
        assert(is_terminated());
        return *as_a<BrInst>(&_insts.back());
    }

  private:
    std::set<BasicBlock *> _pre_bbs;
    std::set<BasicBlock *> _suc_bbs;
    ilist<Instruction> _insts;
    Function *const _func;

    // avoid push inst back to a terminated block
    void avoid_push_back_when_terminated(const InstIter &it) {
        if (it == _insts.end() and is_terminated())
            throw std::logic_error{"trying to push back to a terminated block"};
    }

    // for BrInst use, better to leave them private!
    static void link(BasicBlock *source, BasicBlock *dest);
    static void unlink(BasicBlock *source, BasicBlock *dest);
};

} // namespace ir
