#pragma once

#include <string>

namespace ir {

class Type;
class Module;

class Value {
  public:
    Value(Type *type, std::string &&name) : _type(type), _name(name) {}

    Type *get_type() const { return _type; }
    const std::string &get_name() const { return _name; }

    virtual ~Value() = 0;
    virtual std::string print() const = 0;

    // remove copy constructor
    Value(const Value &) = delete;
    Value &operator=(const Value &) = delete;

  private:
    Type *const _type;
    const std::string _name;
};

inline Value::~Value() {}

} // namespace ir
