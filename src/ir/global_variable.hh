#pragma once

#include "ilist.hh"
#include "module.hh"
#include "value.hh"

#include <string>

namespace ir {

class Constant;
class GlobalVariable : public Value, public ilist<GlobalVariable>::node {
  public:
    static GlobalVariable *get(Type *type, Constant *init, std::string &name,
                               Module *parent);
    Constant *get_init() const { return _init; };
    std::string print() const override;
  private:
    GlobalVariable(Type *type, Constant *init, std::string &name,
                   Module *parent);
    Constant *_init;
    Module *_parent;
};

} // namespace ir
