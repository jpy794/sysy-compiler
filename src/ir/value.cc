#include "value.hh"
#include "err.hh"
#include "user.hh"

using namespace ir;

void Value::add_use(User *user, unsigned int idx) {
    _use_list.emplace_back(user, idx);
}

void Value::replace_all_use_with(Value *new_val) {
    for (auto &use : _use_list) {
        use.user->set_operand(use.op_idx, new_val, false);
        if (new_val != nullptr) {
            // the destructor will pass in nullptr as placeholder
            new_val->add_use(use.user, use.op_idx);
        }
    }
    _use_list.clear();
}

void Value::replace_all_use_with_if(
    Value *new_val, std::function<bool(const Use &)> if_replace) {
    for (auto iter = _use_list.begin(); iter != _use_list.end();) {
        if (if_replace(*iter)) {
            iter->user->set_operand(iter->op_idx, new_val, false);
            new_val->add_use(iter->user, iter->op_idx);
            iter = _use_list.erase(iter);
        } else
            ++iter;
    }
}

void Value::remove_use(User *user, unsigned idx) {
    for (auto iter = _use_list.begin(); iter != _use_list.end(); ++iter) {
        if (iter->user == user and iter->op_idx == idx) {
            _use_list.erase(iter);
            return;
        }
    }
    throw unreachable_error{"the wanted use does not exist!"};
}
