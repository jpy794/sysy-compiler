#pragma once

#include <string>

namespace ir {

class Type;

class Value {
  public:
    Value(Type *type, const std::string &name);
    const Type *get_type() const { return _type; };
    const std::string &get_name() const { return _name; }
    virtual std::string print()const =0;

  private:
    const Type *const _type;
    const std::string _name;
};
}
