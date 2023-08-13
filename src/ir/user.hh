#pragma once

#include <cassert>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "err.hh"
#include "value.hh"

namespace ir {

class User : public Value {
  public:
    User(Type *type, std::string &&name, std::vector<Value *> &&operands)
        : Value(type, std::move(name)), _operands(operands) {
        for (unsigned i = 0; i < _operands.size(); ++i)
            _operands[i]->add_use(this, i);
    }
    ~User() { release_all_use(); }

    // @modify_op_use: if true, maintain use chain auto for old/new op
    virtual void set_operand(size_t idx, Value *value,
                             bool modify_op_use = true) {
        assert(idx < _operands.size());
        if (modify_op_use) {
            _operands[idx]->remove_use(this, idx); // old op
            if (value != nullptr) {                // destructor corner case
                value->add_use(this, idx);         // new op
            }
        }
        _operands[idx] = value;
    }

    void set_operand_for_each_if(
        std::function<std::pair<bool, Value *>(Value *)> check) {
        for (unsigned i = 0; i < _operands.size(); ++i) {
            auto [change, new_value] = check(_operands[i]);
            if (change)
                set_operand(i, new_value);
        }
    }

    void replace_operand(Value *old_val, Value *new_val) {
        set_operand_for_each_if([&](Value *op) -> std::pair<bool, Value *> {
            if (op == old_val) {
                return {true, new_val};
            } else {
                return {false, nullptr};
            }
        });
    }

    void remove_operand(size_t idx) {
        assert(idx < _operands.size());
        for (unsigned i = idx + 1; i < _operands.size(); ++i) {
            _operands[i]->remove_use(this, i);
            _operands[i]->add_use(this, i - 1);
        }
        _operands[idx]->remove_use(this, idx);
        _operands.erase(_operands.begin() + idx);
    }

    Value *get_operand(size_t index) const {
        assert(index < _operands.size());
        return _operands[index];
    }

    // only expose an interface to return const oprands
    const std::vector<Value *> &operands() const { return _operands; }

  protected:
    // sepcial function for PhiInst
    void add_operand(Value *value) {
        value->add_use(this, _operands.size());
        _operands.push_back(value);
    }
    // clear oprands, and suppress the related use chain
    void release_all_use() {
        for (unsigned i = 0; i < _operands.size(); ++i)
            if (_operands[i])
                _operands[i]->remove_use(this, i);
        _operands.clear();
    }

  private:
    std::vector<Value *> _operands;
};

struct Use {
    User *user;
    unsigned op_idx;

    Use(User *u, unsigned i) : user(u), op_idx(i) {}
    Use(const Use &) = delete;
    Use &operator=(const Use &) = delete;

    /* bool operator==(const Use &other) {
     *     return user == other.user and op_idx == other.op_idx;
     * } */
};

} // namespace ir
