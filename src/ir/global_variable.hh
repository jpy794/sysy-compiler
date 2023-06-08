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
    GlobalVariable(Type *type, std::string &&name, Constant *init = nullptr);
    std::string print() const final;

  private:
    // default implicit 0 init
    Constant *_init;
};

} // namespace ir
