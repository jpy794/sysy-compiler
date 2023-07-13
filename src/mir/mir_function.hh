#pragma once

#include "err.hh"
#include "function.hh"
#include "mir_label.hh"
#include "mir_memory.hh"
#include "mir_register.hh"
#include "mir_value.hh"
#include "type.hh"
#include <memory>
#include <string>
#include <vector>

namespace mir {
class Function : public Value {
    friend class ValueManager;

  private:
    bool _is_def;
    std::string _name;
    BasicType _ret_type;
    std::vector<VirtualRegister *> _args;
    std::vector<Label *> _labels; // FIXME the label at 0 is the entry label
    std::vector<StatckObject *> _stack_objects;

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
    VirtualRegister *get_args(size_t idx) { return _args[idx]; }
    bool is_definition() const { return _is_def; }

    std::string get_name() const { return _name; }
    const std::vector<Label *> &get_labels() const { return _labels; }
    void dump(std::ostream &os, const Context &context) const override final;

    template <typename... Args> Label *add_label(Args... args) {
        _labels.push_back(ValueManager::get().create<Label>(args...));
        return _labels.back();
    }

    template <typename... Args> StatckObject *add_stack_object(Args... args) {
        _stack_objects.push_back(
            ValueManager::get().create<StatckObject>(args...));
        return _stack_objects.back();
    }
};
}; // namespace mir
