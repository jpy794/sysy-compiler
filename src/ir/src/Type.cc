#include "Type.hh"
#include <string>

/* ==============FuncType==============*/
string FuncType::print() const {
    string ret;
    ret += _retTp->print();
    ret += " (";
    for (auto paramTp : _paramsTp)
        ret += paramTp->print() + ", ";
    ret[ret.size() - 2] = ')';
    ret = ret.substr(0, ret.size() - 1);
    return ret;
}
bool FuncType::is_valid_return_type(Type *type) {
    return type->is_int_type() or type->is_float_type() or type->is_void_type();
}
bool FuncType::is_valid_argument_type(Type *type) {
    return type->is_int_type() || type->is_pointer_type() ||
           type->is_float_type();
}

/* ==============ArrayType==============*/
string ArrayType::print() const {
    string ret;
    ret = _elementTp->print();

    for (auto iter = _dims.rbegin(); iter != _dims.rend(); ++iter)
        ret = "[" + std::to_string(*iter) + " x " + ret + "]";

    return ret;
}
