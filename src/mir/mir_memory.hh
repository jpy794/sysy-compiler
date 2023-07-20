#pragma once

#include "constant.hh"
#include "err.hh"
#include "global_variable.hh"
#include "ilist.hh"
#include "mir_config.hh"
#include "mir_register.hh"
#include "mir_value.hh"
#include "type.hh"

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

namespace mir {

class MemObject : public Value {
  protected:
    const BasicType _type;
    const std::size_t _size;

    explicit MemObject(BasicType type, std::size_t size)
        : _type(type), _size(size) {}

  public:
    std::size_t get_size() const { return _size; }
    BasicType get_type() const { return _type; }
};

// local vars
class StatckObject : public MemObject {
    friend class ValueManager;

  public:
    enum class Reason { Alloca, Spilled, CalleeSave };

  private:
    const std::size_t _align;
    Reason _reason;

  protected:
    StatckObject(BasicType type, std::size_t size, std::size_t align,
                 Reason reason)
        : MemObject(type, size), _align(align), _reason(reason) {
        assert(type != BasicType::VOID);
    }

  public:
    virtual void dump(std::ostream &os, const Context &context) const override;
    std::size_t get_align() const { return _align; }
    Reason get_reason() const { return _reason; }
};

class CalleeSave : public StatckObject {
    friend class ValueManager;

  private:
    Register::RegIDType _regid;

  private:
    CalleeSave(BasicType type, Register::RegIDType id)
        : StatckObject(type, TARGET_MACHINE_SIZE, TARGET_MACHINE_SIZE,
                       Reason::CalleeSave),
          _regid(id) {}

  public:
    void dump(std::ostream &os, const Context &context) const override final;
    bool is_float_reg() const { return _type == BasicType::FLOAT; }
    Register::RegIDType saved_reg_id() const { return _regid; }
};

class GlobalObject : public MemObject {
    friend class ValueManager;

  private:
    std::string _name;
    InitPairs _inits;

  private:
    static size_t _parse_size(const ir::GlobalVariable *global) {
        auto content_type =
            global->get_type()->as<ir::PointerType>()->get_elem_type();
        size_t size = BASIC_TYPE_SIZE;
        if (content_type->is_basic_type())
            ;
        else if (content_type->is<ir::ArrayType>())
            size *= content_type->as<ir::ArrayType>()->get_total_cnt();
        else
            throw unreachable_error{};
        return size;
    }

    static BasicType _parse_type(const ir::GlobalVariable *global) {
        auto content_type =
            global->get_type()->as<ir::PointerType>()->get_elem_type();
        auto element_type = content_type;
        if (content_type->is<ir::ArrayType>())
            element_type = content_type->as<ir::ArrayType>()->get_base_type();
        assert(element_type->is_basic_type());
        return element_type->is<ir::IntType>() ? BasicType::INT
                                               : BasicType::FLOAT;
    }

    GlobalObject(const ir::GlobalVariable *global)
        : MemObject(_parse_type(global), _parse_size(global)),
          _name(global->get_name().substr(1)) {
        // get init value
        auto init = global->get_init();
        if (_size == BASIC_TYPE_SIZE) { // int or float type global
            if (not is_a<ir::ConstZero>(init)) {
                if (_type == BasicType::INT) {
                    auto init_value = as_a<ir::ConstInt>(init)->val();
                    if (init_value)
                        _inits.push_back({0, init_value});
                } else {
                    auto init_value = as_a<ir::ConstFloat>(init)->val();
                    if (init_value)
                        _inits.push_back({0, init_value});
                }
            }
        } else { // global is an array
            if (not is_a<ir::ConstZero>(init))
                flatten_array(as_a<ir::ConstArray>(init), _inits);
        }
    }

  public:
    void dump(std::ostream &os, const Context &context) const override final;
    const InitPairs &get_init() const { return _inits; }
};

}; // namespace mir
