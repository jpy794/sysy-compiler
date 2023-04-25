#pragma once

#include <string>
using std::string;

class Module {
  public:
    explicit Module(string &name);
    // TODO
    // - global variables
    void add_global_variable(GlobalVariable* g);
    // - Functions: declaration, defination
    // - symbol table: maybe unnecassary
    // - Types: give each type a unique address, for convenience of equal-judge
    Type* get_int_type();
    Type* get_float_type();
    Type* get_array_type(Type* container, size_t size);
    // - and so on

  private:
    string _name;
    std::list<GlobalVariable*> global_var_;
};
