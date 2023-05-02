#pragma once

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "constant.hh"
#include "function.hh"
#include "global_variable.hh"
#include "ilist.hh"
#include "type.hh"
#include "unordered_map"
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
    Type *get_int1_type() const { return _int1_ty.get(); }
    Type *get_int32_type() const { return _int32_ty.get(); }
    Type *get_float_type() const { return _float_ty.get(); }
    Type *get_label_type() const { return _label_ty.get(); }
    Type *get_void_type() const { return _void_ty.get(); }
    Type *get_array_type(Type *container, const std::vector<unsigned> &dims);
    Type *get_pointer_type(Type *element);
    Type *get_function_type(Type *ret, const std::vector<Type *> &params);
    // - and so on
    ConstantInt *be_cached(int val);
    ConstantInt *be_cached(bool val);
    ConstantFloat *be_cached(float val);

  private:
    std::string _name;
    std::unique_ptr<IntType> _int1_ty;
    std::unique_ptr<IntType> _int32_ty;
    std::unique_ptr<FloatType> _float_ty;
    std::unique_ptr<LabelType> _label_ty;
    std::unique_ptr<VoidType> _void_ty;

    std::map<std::pair<Type *, std::vector<Type *>>, std::unique_ptr<FuncType>>
        _func_ty_map;
    std::map<std::pair<Type *, std::vector<unsigned>>,
             std::unique_ptr<ArrayType>>
        _arr_ty_map;
    std::map<Type *, std::unique_ptr<PointerType>> _ptr_ty_map;

    ilist<GlobalVariable> _global_var;
    ilist<Function> _funcs;
    std::unordered_map<int, std::unique_ptr<ConstantInt>> _cached_int;
    std::unordered_map<bool, std::unique_ptr<ConstantInt>> _cached_bool;
    std::unordered_map<float, std::unique_ptr<ConstantFloat>> _cached_float;
};

} // namespace ir
