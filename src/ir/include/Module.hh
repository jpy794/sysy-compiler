#pragma once

#include <string>
using std::string;

class Module {
  public:
    explicit Module(string &name);
    // TODO
    // - global variables
    // - Functions: declaration, defination
    // - symbol table: maybe unnecassary
    // - Types: give each type a unique address, for convenience of equal-judge
    // - and so on

  private:
    string _name;
};
