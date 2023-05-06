#include "type.hh"

using namespace std;
using namespace ir;

bool Type::is_reg_type() {
    return is_a<VoidType>() || is_a<IntType>() || is_a<FloatType>() ||
           is_a<IntType>();
}

bool Type::is_legal_ret_type() {
    // sysy does not allow returning pointers
    return is_reg_type() && not is_a<PointerType>();
}

bool Type::is_legal_param_type() { return is_reg_type(); }

// printers

string FuncType::print() const {
    // we do not need this for now
    throw unreachable_error{};

    string ret;
    ret += _ret_type->print();
    ret += " (";
    for (size_t i = 0; i < _param_types.size(); i++) {
        if (i != _param_types.size() - 1) {
            ret += _param_types[i]->print() + ", ";
        }
    }
    ret += ')';
    return ret;
}
