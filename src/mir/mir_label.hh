#pragma once

#include "ilist.hh"
#include "mir_instruction.hh"
#include "mir_value.hh"

#include <algorithm>
#include <cassert>
#include <string>
#include <vector>

namespace mir {

class Label : public Value {
    friend class ValueManager;

  public:
    enum class LabelType { Normal, Entry, Exit };

  private:
    LabelType _type{LabelType::Normal};
    std::string _name;
    std::vector<Label *> _prev_labels;
    std::vector<Label *> _succ_labels;
    ilist<Instruction> _insts;
    Instruction *_first_branch{nullptr};

    Label(std::string name) : _name(name) {}
    Label(LabelType type, std::string name) : _type(type), _name(name) {}

  public:
    void dump(std::ostream &os, const Context &context) const override final;
    void add_prev(Label *prev) { _prev_labels.push_back(prev); }
    void add_succ(Label *succ) { _succ_labels.push_back(succ); }
    Instruction *get_first_branch() { return _first_branch; }
    const ilist<Instruction> &get_insts() const { return _insts; }
    ilist<Instruction> &get_insts() { return _insts; }
    const std::vector<Label *> &get_prev() const { return _prev_labels; }
    const std::vector<Label *> &get_succ() const { return _succ_labels; }
    const LabelType get_type() const { return _type; }
    const std::string &get_name() const { return _name; }

    void rm_prev(Label *prev) {
        auto iter = std::find(_prev_labels.begin(), _prev_labels.end(), prev);
        assert(iter != _prev_labels.end());
        _prev_labels.erase(iter);
    }
    void rm_succ(Label *prev) {
        auto iter = std::find(_succ_labels.begin(), _succ_labels.end(), prev);
        assert(iter != _succ_labels.end());
        _succ_labels.erase(iter);
    }

    Instruction &add_inst(MIR_INST inst_type, std::vector<Value *> operands,
                          bool partial = false) {
        _insts.emplace_back(inst_type, operands, partial);
        auto &new_inst = _insts.back();
        if (new_inst.is_branch_inst() and _first_branch == nullptr)
            _first_branch = &new_inst;
        return new_inst;
    }

    Instruction &insert_before(Instruction *inst, MIR_INST inst_type,
                               std::vector<Value *> operands,
                               bool partial = false) {
        return *_insts.emplace(inst, inst_type, operands, partial);
    }

  private:
};

}; // namespace mir
