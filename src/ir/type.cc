#include "type.hh"
#include "utils.hh"
#include <cassert>

using namespace std;
using namespace ir;

bool Type::is_basic_type() { return is<IntType>() or is<FloatType>(); }

bool Type::is_legal_ret_type() { return is_basic_type() or is<VoidType>(); }

bool Type::is_legal_param_type() {
    return is_basic_type() or is<PointerType>();
}

Type *ArrayType::_get_base_type() const {
    auto arrty = this;
    while (arrty->get_elem_type()->is<ArrayType>())
        arrty = arrty->get_elem_type()->as<ArrayType>();
    assert(arrty->get_elem_type()->is_basic_type());
    return arrty->get_elem_type();
}
size_t ArrayType::_get_total_cnt() const {
    auto arrty = this;
    size_t total_cnt = this->get_elem_cnt();
    while (arrty->get_elem_type()->is<ArrayType>()) {
        arrty = arrty->get_elem_type()->as<ArrayType>();
        total_cnt *= arrty->get_elem_cnt();
    }
    return total_cnt;
}
