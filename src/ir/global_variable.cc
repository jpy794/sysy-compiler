#include "global_variable.hh"
#include "constant.hh"
using namespace ir;

GlobalVariable::GlobalVariable(Type *type, Constant *init, std::string &name,
                               Module *parent)
    : Value(type, name), _init(init), _parent(parent){};
GlobalVariable *GlobalVariable::get(Type *type, Constant *init,
                                    std::string &name, Module *m) {
    GlobalVariable *global_v = new GlobalVariable(type, init, name, m);
    m->add_global_variable(global_v);
    return global_v;
}
std::string GlobalVariable::print() const {
    return this->get_type()->print() + " " + this->get_name() + " = " +
           _init->print();
}
