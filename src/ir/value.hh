#pragma once

#include "utils.hh"

#include <string>

namespace ir {

class Type;
class Module;

class Value {
  public:
    Value(Type *type, std::string &&name) : _type(type), _name(name) {}

    Type *get_type() const { return _type; }
    const std::string &get_name() const { return _name; }

    template <typename Derived> bool is() { return ::is_a<Derived>(this); }
    template <typename Derived> Derived *as() { return ::as_a<Derived>(this); }

    virtual ~Value() = default;
    virtual std::string print() const = 0;

    // remove copy constructor
    Value(const Value &) = delete;
    Value &operator=(const Value &) = delete;

  private:
    Type *const _type;
    const std::string _name;
};

} // namespace ir
