#include "basic_block.hh"
#include "function.hh"
#include <string>

using namespace ir;

BasicBlock::BasicBlock(Function *parent)
    : Value(parent->get_module()->get_label_type(), "label" + std::to_string(parent->get_seq())), _parent(parent) {
    parent->add_basic_block(this);
}
BasicBlock *BasicBlock::create(Function *parent) {
    return new BasicBlock(parent);
}
void BasicBlock::add_instruction(Instruction *instr) {
    _instr_list.push_back(instr);
}