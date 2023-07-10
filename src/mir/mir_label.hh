#pragma once

#include "ilist.hh"
#include "mir_instruction.hh"
#include "mir_value.hh"

#include <string>
#include <vector>

namespace mir {

class Label : public Value {
    friend class ValueManager;

    std::string _name;
    std::vector<Label *> _prev_labels;
    std::vector<Label *> _succ_labels;
    ilist<Instruction> _insts;

    Label(std::string name) : _name(name) {}

  public:
    void dump(std::ostream &os, const MIRContext &context) const final {
        switch (context.role) {
        case Role::Full: {
            os << _name << ":\n";
            for (auto &inst : _insts) {
                os << "\t";
                inst.dump(os, context);
                os << "\n";
            }
        } break;
        case Role::NameOnly:
            os << _name;
            break;
        }
    }
    void add_prev(Label *prev) { _prev_labels.push_back(prev); }
    void add_succ(Label *succ) { _succ_labels.push_back(succ); }

    Instruction &add_inst(MIR_INST inst_type, std::vector<Value *> operands,
                          bool partial = false) {
        _insts.emplace_back(inst_type, operands, partial);
        return _insts.back();
    }
};

}; // namespace mir
