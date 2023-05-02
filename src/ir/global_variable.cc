#include "global_variable.hh"
#include "constant.hh"
#include "module.hh"
#include "type.hh"
#include <utility>
using namespace ir;

GlobalVariable::GlobalVariable(Type *type, Constant *init, std::string &&name,
                               Module *parent)
    : Value(parent, parent->get_pointer_type(type), std::move(name)),
      _init(init), _parent(parent){};
GlobalVariable *GlobalVariable::get(Type *type, Constant *init,
                                    std::string &&name, Module *m) {
    GlobalVariable *global_v =
        new GlobalVariable(type, init, std::move(name), m);
    m->add_global_variable(global_v);
    return global_v;
}
std::string GlobalVariable::print() const {
    std::string init_ir;
    if (_init)
        init_ir = _init->print();
    else
        init_ir = "";
    return print_op(this) + " = global " +
           dynamic_cast<PointerType *>(this->get_type())
               ->get_element_type()
               ->print() +
           init_ir;
}
