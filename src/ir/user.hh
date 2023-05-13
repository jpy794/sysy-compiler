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
        : Value(type, std::move(name)), _operands(operands),
          _op_num(operands.size()) {}

    virtual void set_operand(size_t index, Value *value) {
        throw unreachable_error{"not implemented"};
        assert(index < _op_num);
        _operands[index] = value;
    }

    // const method
    const std::vector<Value *> &operands() const { return _operands; }

    Value *get_operand(size_t index) const {
        assert(index < _op_num);
        return _operands[index];
    }

  private:
    std::vector<Value *> _operands;
    const size_t _op_num;
};

} // namespace ir
