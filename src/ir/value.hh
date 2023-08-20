#pragma once

#include "utils.hh"

#include <functional>
#include <list>
#include <string>

namespace ir {

class Type;
class Module;

class User;
struct Use;

class Value {
  public:
    Value(Type *type, std::string &&name) : _type(type), _name(name) {}
    ~Value() { replace_all_use_with(nullptr); }

    Type *get_type() const { return _type; }
    const std::string &get_name() const { return _name; }

    template <typename Derived> bool is() { return ::is_a<Derived>(this); }
    template <typename Derived> Derived *as() { return ::as_a<Derived>(this); }

    virtual std::string print() const = 0;

    // remove copy constructor
    Value(const Value &) = delete;
    Value &operator=(const Value &) = delete;

    // functions to maintain use list
    void add_use(User *user, unsigned idx);
    void remove_use(User *user, unsigned idx);
    void replace_all_use_with(Value *new_val);
    void replace_all_use_with_if(Value *new_val,
                                 std::function<bool(const Use &)> if_replace);
    const std::list<Use> &get_use_list() const { return _use_list; }
    // std::list<Use>::iterator remove_use()

  protected:
    void change_type(Type *type) { _type = type; }

  private:
    Type *_type;
    const std::string _name;
    std::list<Use> _use_list;
};

} // namespace ir
