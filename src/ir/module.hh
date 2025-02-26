#pragma once

#include <string>

#include "function.hh"
#include "global_variable.hh"
#include "ilist.hh"
#include "type.hh"

// TODO:
// singleton for type -- done
// delete the types in case memory leak
// no need to cache constant, but simply use operator== won't work?
// should we use value ptr for constant ? live interval of module type const ?

namespace ir {

class Module {
  public:
    explicit Module(std::string &&name) : _name(name) {}

    // creaters
    template <typename... Args>
    GlobalVariable *create_global_var(Args &&...args) {
        _global_vars.emplace_back(std::forward<Args>(args)...);
        return &_global_vars.back();
    }

    template <typename... Args> Function *create_func(Args &&...args) {
        _funcs.emplace_back(std::forward<Args>(args)...);
        return &_funcs.back();
    }

    std::string print() const;

    void set_main(Function *main) { _main_func = main; }
    Function *get_main() const { return _main_func; }

    // non-const
    ilist<GlobalVariable> &global_vars() { return _global_vars; }
    ilist<Function> &functions() { return _funcs; }

    // const
    const ilist<GlobalVariable> &get_global_vars() const {
        return _global_vars;
    }
    const ilist<Function> &get_functions() const { return _funcs; }

  private:
    std::string _name;
    Function *_main_func{nullptr};

    ilist<GlobalVariable> _global_vars;
    ilist<Function> _funcs;
};

} // namespace ir
