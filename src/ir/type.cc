#include "type.hh"
#include "utils.hh"

using namespace std;
using namespace ir;

bool Type::is_basic_type() { return is_a<IntType>() or is_a<FloatType>(); }

bool Type::is_legal_ret_type() { return is_basic_type() or is_a<VoidType>(); }

bool Type::is_legal_param_type() {
    return is_basic_type() or is_a<PointerType>();
}

bool ArrayType::_is_legal_array() {
    auto arrty = this;
    while (arrty->get_elem_type()->is_a<ArrayType>())
        arrty = arrty->get_elem_type()->as_a<ArrayType>();
    return arrty->get_elem_type()->is_basic_type();
}
