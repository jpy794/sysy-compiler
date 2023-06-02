#pragma once

#include "constant.hh"
#include "ilist.hh"
#include "type.hh"

#include <algorithm>
#include <string>
#include <variant>

namespace ir {

class Constant;

class GlobalVariable : public Value, public ilist<GlobalVariable>::node {
  public:
    GlobalVariable(Type *type, std::string &&name,
                   const std::vector<std::pair<size_t, Constant *>> &init = {});
    Constant *get_init(size_t idx) const {
        if (_init.find(idx) != _init.end())
            return _init.at(idx);

        // check base type
        Type *base_type{nullptr};
        if (get_type()->is_basic_type())
            base_type = get_type();
        else if (get_type()->is<ArrayType>())
            base_type = get_type()->as<ArrayType>()->get_base_type();
        else
            throw unreachable_error{};

        // return implicit 0
        if (base_type->is<IntType>())
            return Constants::get().int_const(0);
        else
            return Constants::get().float_const(0);
    };
    std::string print() const final;

  private:
    // bool is a quick judge for zero init
    std::pair<bool, std::string>
    _gen_initializer(Type *type, size_t off,
                     std::map<size_t, Constant *>::const_iterator &iter) const;
    static std::string _gen_zeroinitializer(Type *type, ConstArray *init,
                                            size_t &index);
    // default implicit 0 init
    std::map<size_t, Constant *> _init;
};

} // namespace ir
