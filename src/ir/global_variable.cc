#include "global_variable.hh"
#include "constant.hh"
#include "module.hh"
#include "type.hh"
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>

using namespace ir;
using namespace std;

GlobalVariable::GlobalVariable(Type *type, string &&name, Constant *init)
    : Value(Types::get().ptr_type(type), "@" + name), _init(init) {
    if (init == nullptr) {
        init = Constants::get().zero_const(type);
    }
}

string GlobalVariable::print() const {
    return get_name() + " = global " +
           get_type()->as<PointerType>()->get_elem_type()->print() + " " +
           _init->get_name();
}
