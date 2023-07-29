#include "basic_block.hh"
#include "function.hh"

#include <string>

using namespace ir;

BasicBlock::BasicBlock(Function *func)
    : Value(Types::get().label_type(),
            "label" + std::to_string(func->get_inst_seq())),
      _func(func) {}

std::string BasicBlock::print() const {
    std::string bb_ir = "";
    std::string pre_bbs = "";
    for (const auto &pre_bb : get_pre_bbs()) {
        pre_bbs += pre_bb->get_name() + " ";
    }
    if (pre_bbs.size() != 0)
        bb_ir = this->get_name() + ":\t\t\t" + ";pre_bbs=" + pre_bbs + "\n";
    else
        bb_ir = this->get_name() + ":\n";
    for (auto &inst : get_insts()) {
        bb_ir += "\t" + inst.print() + "\n";
    }
    return bb_ir;
}
