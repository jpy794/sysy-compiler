#pragma once

#include <string>

namespace ir {

class Type;

class Value {
  public:
    Value(Type *type, const std::string &name);
    const Type *get_type() const { return _type; };
    const std::string &get_name() const { return _name; }

  private:
    const Type *const _type;
    const std::string _name;
};

} // namespace ir
