#include "basic_block.hh"
#include "function.hh"
#include "ilist.hh"
#include "instruction.hh"

#include <cassert>
#include <string>

using namespace ir;
using namespace std;

BasicBlock::BasicBlock(Function *func)
    : Value(Types::get().label_type(),
            "label" + to_string(func->get_inst_seq())),
      _func(func) {}

bool BasicBlock::is_terminated() const {
    return _insts.size() != 0 and (is_a<const BrInst>(&_insts.back()) or
                                   is_a<const RetInst>(&_insts.back()));
}

string BasicBlock::print() const {
    string bb_ir = "";
    string pre_bbs = "";
    for (const auto &pre_bb : _pre_bbs) {
        pre_bbs += pre_bb->get_name() + " ";
    }
    if (pre_bbs.size() != 0)
        bb_ir = get_name() + ":\t\t\t" + ";pre_bbs=" + pre_bbs + "\n";
    else
        bb_ir = get_name() + ":\n";
    for (auto &inst : _insts) {
        bb_ir += "\t" + inst.print() + "\n";
    }
    return bb_ir;
}

Instruction *BasicBlock::clone_inst(const InstIter &it, Instruction *other) {
    auto other_bb = other->get_parent();
    assert(other_bb->get_func() == _func);
    avoid_push_back_when_terminated(it);
    // check for RetInst and BrInst
    if (other->is<RetInst>() or other->is<BrInst>())
        assert(it == _insts.end());

    /* About use chain and prev/succ:
     * clone calls constructor of Instruction, which maintains use chain */
    auto inst = other->clone(this);
    _insts.insert(it, inst);
    return inst;
}

// we should maintain prev/succ and termination info carefully,
// cause this function does not arouse constructor/destructor
Instruction *BasicBlock::move_inst(const InstIter &it, Instruction *other) {
    auto other_bb = other->get_parent();
    assert(other_bb->get_func() == _func);
    avoid_push_back_when_terminated(it);
    // check for RetInst and BrInst
    if (other->is<RetInst>() or other->is<BrInst>())
        assert(it == _insts.end());
    // check for BrInst
    if (other->is<BrInst>()) {
        assert(this->_suc_bbs.empty());
        BasicBlock *old_src = other_bb;
        BasicBlock *new_src = this;
        for (auto boba : other->operands()) {
            if (is_a<BasicBlock>(boba)) {
                BasicBlock *dest = as_a<BasicBlock>(boba);
                // maintain prev/succ links
                BasicBlock::unlink(old_src, dest);
                BasicBlock::link(new_src, dest);
            }
        }
        assert(other_bb->_suc_bbs.size() == 0);
    }

    auto inst = other_bb->insts().release(other);
    inst->_parent = this;
    return &*_insts.insert(it, inst);
}

BasicBlock::InstIter BasicBlock::erase_inst(Instruction *inst) {
    if (inst->get_use_list().size())
        throw logic_error{
            "you cannot erase this inst because there is someone using it"};
    return _insts.erase(inst);
}

void BasicBlock::link(BasicBlock *src, BasicBlock *dest) {
    auto &src_succ_bbs = src->_suc_bbs;
    auto &dest_prev_bbs = dest->_pre_bbs;
    assert(not contains(src_succ_bbs, dest));
    assert(not contains(dest_prev_bbs, src));
    // add dest bb to src's succ
    src_succ_bbs.push_back(dest);
    // add src bb to dest's succ
    dest_prev_bbs.push_back(src);
}

void BasicBlock::unlink(BasicBlock *src, BasicBlock *dest) {
    auto &src_succ_bbs = src->_suc_bbs;
    auto &dest_prev_bbs = dest->_pre_bbs;
    { // remove dest bb from src's succ
        auto iter = find(src_succ_bbs.begin(), src_succ_bbs.end(), dest);
        assert(iter != src_succ_bbs.end());
        src_succ_bbs.erase(iter);
    }
    { // remove src bb from dest's pre
        auto iter = find(dest_prev_bbs.begin(), dest_prev_bbs.end(), src);
        assert(iter != dest_prev_bbs.end());
        dest_prev_bbs.erase(iter);
    }
}
