#pragma once

#include "constant.hh"
#include "ilist.hh"
#include "type.hh"
#include "value.hh"

#include <string>

namespace ir {

class Constant;

class GlobalVariable : public Value, public ilist<GlobalVariable>::node {
  public:
    GlobalVariable(Type *type, Constant *init, std::string &&name);
    Constant *get_init() const { return _init; };
    std::string print() const final;

  private:
    static std::string _gen_zeroinitializer(Type *type, ConstArray *init,
                                            size_t &index);
    Constant *const _init;
};

} // namespace ir
