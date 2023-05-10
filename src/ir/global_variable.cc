#include "global_variable.hh"
#include "constant.hh"
#include "module.hh"
#include "type.hh"

using namespace ir;

GlobalVariable::GlobalVariable(Type *type, Constant *init, std::string &&name)
    : Value(Types::get().ptr_type(type), "@" + name), _init(init){};

std::string GlobalVariable::print() const {
    std::string init_ir;
    if (_init)
        init_ir = " " + _init->print();
    else
        init_ir = "";
    return get_name() + " = global " +
           dynamic_cast<PointerType *>(get_type())->get_elem_type()->print() +
           init_ir;
}
