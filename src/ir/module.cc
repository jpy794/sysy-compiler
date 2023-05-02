#include "module.hh"
#include "constant.hh"
#include "type.hh"
#include <memory>
#include <utility>

using namespace ir;
using namespace std;

Module::Module(string &&name) : _name(name) {
    _int1_ty = make_unique<IntType>(1);
    _int32_ty = make_unique<IntType>(32);
    _float_ty = make_unique<FloatType>();
    _void_ty = make_unique<VoidType>();
    _label_ty = make_unique<LabelType>();
}

void Module::add_global_variable(GlobalVariable *gv) {
    _global_var.push_back(gv);
}

void Module::add_function(Function *func) { _funcs.push_back(func); }

Type *Module::get_array_type(Type *container, const vector<unsigned> &&dims) {
    auto key = make_pair(container, dims);
    auto iter = _arr_ty_map.find(key);
    if (iter == _arr_ty_map.end()) {
        auto new_ty = new ArrayType(container, dims);
        _arr_ty_map[key] = make_unique<ArrayType>(container, dims);
        return new_ty;
    } else
        return iter->second.get();
}

Type *Module::get_pointer_type(Type *elementTp) {
    auto iter = _ptr_ty_map.find(elementTp);
    if (iter == _ptr_ty_map.end()) {
        auto new_ty = new PointerType(elementTp);
        _ptr_ty_map[elementTp] = make_unique<PointerType>(elementTp);
        return new_ty;
    } else
        return iter->second.get();
}

Type *Module::get_function_type(Type *ret, const vector<Type *> &&params) {
    auto key = make_pair(ret, params);
    auto iter = _func_ty_map.find(key);
    if (iter == _func_ty_map.end()) {
        auto new_ty = new FuncType(ret, params);
        _func_ty_map[key] = make_unique<FuncType>(ret, params);
        return new_ty;
    } else
        return iter->second.get();
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
