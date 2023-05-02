#pragma once

#include <string>

namespace ir {

class Type;
class Module;

class Value {
  public:
    Value(Module *module, Type *type, std::string &&name)
        : _module(module), _type(type), _name(name) {}

    Module *module() const { return _module; }
    Type *get_type() const { return _type; }
    const std::string &get_name() const { return _name; }

    virtual ~Value() = 0;
    virtual std::string print() const = 0;

    // remove copy constructor
    Value(const Value &) = delete;
    Value &operator=(const Value &) = delete;

  private:
    Module *const _module;
    Type *const _type;
    const std::string _name;
};

} // namespace ir
