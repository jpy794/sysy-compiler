#include "global_variable.hh"
#include "constant.hh"
#include "module.hh"
#include "type.hh"

using namespace ir;

GlobalVariable::GlobalVariable(Module *module, Type *type, Constant *init,
                               std::string &&name)
    : Value(module, module->get_pointer_type(type), std::move(name)),
      _init(init){};

std::string GlobalVariable::print() const {
    std::string init_ir;
    if (_init)
        init_ir = " " + _init->print();
    else
        init_ir = "";
    return print_op(this) + " = global " +
           dynamic_cast<PointerType *>(this->get_type())
               ->get_element_type()
               ->print() +
           init_ir;
}
