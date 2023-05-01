#pragma once

#include <string>

namespace ir {

class Type;

class Value {
  public:
    Value(Type *type, const std::string &name);
    const Type *type() const { return _type; };
    const std::string &name() const { return _name; }

  private:
    const Type *const _type;
    const std::string _name;
};

} // namespace ir
