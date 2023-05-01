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
std::string BasicBlock::print() const{
    std::string bb_ir="";
    std::string pre_bbs="";
    for(const auto& pre_bb : this->get_pre_basic_blocks()){
        pre_bbs += pre_bb->get_name() + " ";
    }
    if(pre_bbs.size()!=0)bb_ir = this->get_name() + ":\t\t\t" + "pre_bbs=" + pre_bbs + "\n";
    else bb_ir = this->get_name() + ":\n";
    for(auto& inst : this->_instr_list){
        bb_ir+= "\t" + inst.print() + "\n";
    }
    return bb_ir;
}