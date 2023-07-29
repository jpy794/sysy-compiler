#pragma once

#include "err.hh"
#include "function.hh"
#include "mir_config.hh"
#include "mir_label.hh"
#include "mir_memory.hh"
#include "mir_register.hh"
#include "mir_value.hh"
#include "type.hh"
#include <cassert>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace mir {

using Offset = size_t;

struct FrameInfo {
    Offset size;
    std::map<StackObject *, Offset> offset;
};

class Function : public Value {
    friend class ValueManager;

  private:
    bool _is_def;
    std::string _name;
    BasicType _ret_type;
    std::vector<VirtualRegister *> _args;
    std::deque<Label *> _labels; // FIXME the label at 0 is the entry label

    // objects in stack
    std::vector<StackObject *> _local_vars;
    std::vector<CalleeSave *> _callee_saves;

    // frame info
    FrameInfo _frame;

  private:
    Function(const ir::Function *func)
        : _is_def(!func->is_external), _name(func->get_name().substr(1)) {
        auto ret_type = func->get_return_type();
        if (ret_type->is<ir::VoidType>())
            _ret_type = BasicType::VOID;
        else if (ret_type->is<ir::IntType>())
            _ret_type = BasicType::INT;
        else if (ret_type->is<ir::FloatType>())
            _ret_type = BasicType::FLOAT;
        else
            throw unreachable_error{};
        if (_is_def)
            for (auto &arg : func->get_args()) {
                auto type = arg->get_type();
                if (type->is<ir::IntType>() or type->is<ir::PointerType>()) {
                    _args.push_back(ValueManager::get().create<IVReg>());
                } else if (type->is<ir::FloatType>()) {
                    _args.push_back(ValueManager::get().create<FVReg>());
                } else
                    throw unreachable_error{};
            }
    }

    Function(bool def, std::string name, BasicType ret)
        : _is_def(def), _name(name), _ret_type(ret) {}

  public:
    BasicType get_ret_type() const { return _ret_type; }
    bool is_definition() const { return _is_def; }
    VirtualRegister *get_args(size_t idx) { return _args[idx]; }
    const std::string &get_name() const { return _name; }
    const decltype(_args) &get_args() { return _args; }
    const decltype(_labels) &get_labels() const { return _labels; }
    const decltype(_frame) &get_frame() const { return _frame; }
    const decltype(_callee_saves) &get_callee_saves() const {
        return _callee_saves;
    }

    void dump(std::ostream &os, const Context &context) const override final;

    void allocate_location() {
        Offset off = 0;
        for (auto save : _callee_saves) {
            off += save->get_size();
            off = ALIGN(off, save->get_align());
            _frame.offset[save] = off;
        }
        for (auto local : _local_vars) {
            off += local->get_size();
            off = ALIGN(off, local->get_align());
            _frame.offset[local] = off;
        }
        off = ALIGN(off, SP_ALIGNMENT);
        for (auto &[k, v] : _frame.offset)
            _frame.offset.at(k) = off - v;
        _frame.size = off;
    }

    Label *create_entry() {
        assert(_labels.front()->get_type() != Label::LabelType::Entry);
        _labels.push_front(
            ValueManager::get().create<Label>(Label::LabelType::Entry, ""));
        return _labels.front();
    }
    Label *create_exit() {
        assert(_labels.back()->get_type() != Label::LabelType::Exit);
        _labels.push_back(ValueManager::get().create<Label>(
            Label::LabelType::Exit, _name + "_exit"));
        return _labels.back();
    }
    template <typename... Args> Label *add_label(Args... args) {
        _labels.push_back(ValueManager::get().create<Label>(args...));
        return _labels.back();
    }
    template <typename... Args> StackObject *add_local_var(Args... args) {
        _local_vars.push_back(ValueManager::get().create<StackObject>(args...));
        return _local_vars.back();
    }
    template <typename... Args> CalleeSave *add_callee_save(Args... args) {
        _callee_saves.push_back(
            ValueManager::get().create<CalleeSave>(args...));
        return _callee_saves.back();
    }
};
}; // namespace mir
