#pragma once

#include <string>
#include <vector>

#include "value.hh"

namespace ir {

class User : public Value {
  public:
    User(Type *type, const std::string &name, std::vector<Value *> &&operands)
        : Value(type, name), _operands(operands) {}

    const std::vector<Value *> &operands() const { return _operands; }
    std::vector<Value *> &operands() { return _operands; }

  private:
    std::vector<Value *> _operands;
};

} // namespace ir
