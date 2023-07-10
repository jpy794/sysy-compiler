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

// TODO

class MemObject : public Value {
  protected:
    const std::size_t _size;

    explicit MemObject(std::size_t size) : _size(size) {}

  public:
    std::size_t get_size() const { return _size; }
};

class StatckObject : public MemObject {
    friend class ValueManager;

  private:
    const std::size_t _align;

  private:
    StatckObject(std::size_t size, std::size_t align)
        : MemObject(size), _align(align) {}

  public:
    void dump(std::ostream &os, const MIRContext &context) const final {
        switch (context.stage) {
        case Stage::stage1: {
            os << "@stack-object";
            break;
        }
        case Stage::stage2:
            throw not_implemented_error{};
        }
    }
    std::size_t get_align() const { return _align; }
};

class GlobalObject : public MemObject {
    friend class ValueManager;

  private:
    BasicType _type;
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
        : MemObject(_parse_size(global)), _type(_parse_type(global)),
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
    void dump(std::ostream &os, const MIRContext &context) const final {
        switch (context.role) {
        case Role::Full: {
            os << _name << ":\n";
            size_t off = 0; // byte offset
            for (auto [idx, value] : _inits) {
                if (off < idx * BASIC_TYPE_SIZE) {
                    os << "\t.zero "
                       << std::to_string(idx * BASIC_TYPE_SIZE - off) << "\n";
                }
                switch (_type) {
                case BasicType::VOID:
                    throw unreachable_error{};
                case BasicType::INT:
                    os << "\t.word " << std::get<int>(value) << "\n";
                    break;
                case BasicType::FLOAT: {
                    float v = std::get<float>(value);
                    os << "\t.word 0x" << std::hex << std::uppercase
                       << *reinterpret_cast<uint32_t *>(&v) << " # float " << v
                       << "\n";
                    break;
                }
                }
                off = (idx + 1) * BASIC_TYPE_SIZE;
            }
            assert(off <= _size);
            if (off != _size)
                os << "\t.zero " << std::to_string(_size - off) << "\n";
        } break;
        case Role::NameOnly:
            os << _name;
            break;
        }
    }
    const InitPairs &get_init() const { return _inits; }
    BasicType get_type() const { return _type; }
};

}; // namespace mir
