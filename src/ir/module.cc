#include "module.hh"

using namespace ir;

Module::Module(std::string &name) : _name(name) {
    _int_ty = std::make_unique<IntType>(this, 32);
    _float_ty = std::make_unique<FloatType>(this);
    _void_ty = std::make_unique<VoidType>(this);
    _label_ty = std::make_unique<LabelType>(this);
}

void Module::add_global_variable(GlobalVariable *gv) {
    _global_var.push_back(gv);
}

void Module::add_function(Function *func) { _funcs.push_back(func); }

// Type* Module::get_array_type(Type* container, std::vector<unsigned>&& dims){
//     return new ArrayType(this, container, dims);
// }

Type *Module::get_array_type(Type *container, unsigned length) {
    return new ArrayType(this, container, length);
}

Type *Module::get_pointer_type(Type *elementTp) {
    return new PointerType(this, elementTp);
}

Type *Module::get_function_type(Type *ret, const std::vector<Type *> &params) {
    return new FuncType(this, ret, params);
}
