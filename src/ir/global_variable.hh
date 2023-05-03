#pragma once

#include "ilist.hh"
#include "type.hh"
#include "value.hh"

#include <string>

namespace ir {

class Constant;

class GlobalVariable : public Value, public ilist<GlobalVariable>::node {
  public:
    GlobalVariable(Module *module, Type *type, Constant *init,
                   std::string &&name);
    Constant *get_init() const { return _init; };
    std::string print() const final;

  private:
    Constant *const _init;
};

} // namespace ir
