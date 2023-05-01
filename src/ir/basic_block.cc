#include "basic_block.hh"
#include "function.hh"

using namespace ir;

BasicBlock::BasicBlock(const std::string &name, Function *parent)
    : Value(parent->get_module()->get_label_type(), name), _parent(parent) {
    parent->add_basic_block(this);
}
BasicBlock *BasicBlock::create(const std::string &name, Function *parent) {
    return new BasicBlock(name, parent);
}
void BasicBlock::add_instruction(Instruction *instr) {
    _instr_list.push_back(instr);
}