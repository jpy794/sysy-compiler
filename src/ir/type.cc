#include "type.hh"
#include "utils.hh"

using namespace std;
using namespace ir;

bool Type::is_basic_type() { return is<IntType>() or is<FloatType>(); }

bool Type::is_legal_ret_type() { return is_basic_type() or is<VoidType>(); }

bool Type::is_legal_param_type() {
    return is_basic_type() or is<PointerType>();
}

bool ArrayType::_is_legal_array() {
    auto arrty = this;
    while (arrty->get_elem_type()->is<ArrayType>())
        arrty = arrty->get_elem_type()->as<ArrayType>();
    return arrty->get_elem_type()->is_basic_type();
}
