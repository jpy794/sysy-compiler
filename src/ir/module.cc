#include "module.hh"
#include "constant.hh"
#include "type.hh"
#include <memory>

using namespace ir;

Module::Module(std::string &name) : _name(name) {
    _int1_ty = std::make_unique<IntType>(this, 1);
    _int32_ty = std::make_unique<IntType>(this, 32);
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
ConstantInt *Module::be_cached(bool val) {
    if (this->_cached_bool.find(val) != _cached_bool.end())
        return _cached_bool[val].get();
    else {
        return (_cached_bool[val] = std::unique_ptr<ConstantInt>(
                    new ConstantInt(this->get_int1_type(), val)))
            .get();
    }
}
ConstantInt *Module::be_cached(int val) {
    if (this->_cached_int.find(val) != _cached_int.end())
        return _cached_int[val].get();
    else {
        return (_cached_int[val] = std::unique_ptr<ConstantInt>(
                    new ConstantInt(this->get_int32_type(), val)))
            .get();
    }
}
ConstantFloat *Module::be_cached(float val) {
    if (this->_cached_float.find(val) != _cached_float.end())
        return _cached_float[val].get();
    else {
        return (_cached_float[val] = std::unique_ptr<ConstantFloat>(
                    new ConstantFloat(this->get_float_type(), val)))
            .get();
    }
}