#pragma once

#include <memory>
#include <string>

#include "function.hh"
#include "global_variable.hh"
#include "ilist.hh"
#include "type.hh"
namespace ir {

class Module {
  public:
    explicit Module(std::string &name);
    // TODO
    // - global variables
    void add_global_variable(GlobalVariable *gv);
    // - Functions: declaration, defination
    void add_function(Function *func);
    // - symbol table: maybe unnecassary
    // - Types: give each type a unique address, for convenience of equal-judge
    Type *get_int_type() const { return _int_ty.get(); }
    Type *get_float_type() const { return _float_ty.get(); }
    Type *get_label_type() const { return _label_ty.get(); }
    Type *get_void_type() const { return _void_ty.get(); }
    // Type* get_array_type(Type* container, std::vector<unsigned>&& dims);
    Type *get_array_type(Type *container, unsigned length);
    Type *get_pointer_type(Type *element);
    Type *get_function_type(Type *ret, const std::vector<Type *> &params);
    // - and so on

  private:
    std::string _name;
    std::unique_ptr<IntType> _int_ty;
    std::unique_ptr<FloatType> _float_ty;
    std::unique_ptr<LabelType> _label_ty;
    std::unique_ptr<VoidType> _void_ty;
    ilist<GlobalVariable> _global_var;
    ilist<Function> _funcs;
};

} // namespace ir
