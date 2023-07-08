#pragma once

#include <cassert>
#include <string>
#include <vector>

#include "err.hh"
#include "value.hh"

namespace ir {

class User : public Value {
  public:
    User(Type *type, std::string &&name, std::vector<Value *> &&operands)
        : Value(type, std::move(name)), _operands(operands) {}

    void set_operand(size_t index, Value *value) {
        assert(index < _operands.size());
        _operands[index] = value;
    }

    // const method
    const std::vector<Value *> &operands() const { return _operands; }

    Value *get_operand(size_t index) const {
        assert(index < _operands.size());
        return _operands[index];
    }

  protected:
    void add_operand(Value *value) { _operands.push_back(value); }

  private:
    std::vector<Value *> _operands;
};

} // namespace ir
