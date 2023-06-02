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

GlobalVariable::GlobalVariable(Type *type, string &&name,
                               const vector<pair<size_t, Constant *>> &init)
    : Value(Types::get().ptr_type(type), "@" + name) {
    for (auto &[idx, val] : init)
        _init[idx] = val;
}

string str_of_const_0(Type *type) {
    assert(type->is_basic_type());
    return type->print() + " " +
           (type->is<IntType>() ? Constants::get().int_const(0)->get_name()
                                : Constants::get().float_const(0)->get_name());
}

pair<bool, string> GlobalVariable::_gen_initializer(
    Type *type, size_t off,
    map<size_t, Constant *>::const_iterator &iter) const {
    auto left_is_all_zero = iter == _init.end();
    if (left_is_all_zero) { // no more init value left, all should be zero
        if (type->is_basic_type()) {
            return {true, str_of_const_0(type)};
        } else
            return {true, type->print() + " zeroinitializer"};
    }

    // leaf node
    if (type->is_basic_type()) {
        assert(off <= iter->first);
        if (off == iter->first) {
            auto str = type->print() + " " + iter->second->get_name();
            ++iter;
            return {false, str};
        } else {
            return {true, str_of_const_0(type)};
        }
    }

    // FIXME: this approach may has low performance
    // array type
    auto arr_type = type->as<ArrayType>();
    bool all_zero = true;
    size_t step = arr_type->get_total_cnt() / arr_type->get_elem_cnt();
    string str{""};
    for (size_t i = 0; i < arr_type->get_elem_cnt(); ++i) {
        auto [is_zero, sub_str] =
            _gen_initializer(arr_type->get_elem_type(), off, iter);
        off += step;

        str += sub_str + ", ";
        all_zero &= is_zero;
    }

    if (all_zero) {
        str = type->print() + " zeroinitializer";
        return {true, str};
    } else {
        str = type->print() + " [" + str.substr(0, str.size() - 2) + "]";
        return {false, str};
    }
}

string GlobalVariable::_gen_zeroinitializer(Type *type, ConstArray *init,
                                            size_t &index) {
    if (type->is_basic_type()) {
        return init->array()[index++]->get_name();
    } else {
        size_t elem_cnt = type->as<ArrayType>()->get_elem_cnt();
        auto elem_type = type->as<ArrayType>()->get_elem_type();
        string array;
        bool all_zero = true;
        for (size_t i = 0; i < elem_cnt; i++) {
            auto elem_str = _gen_zeroinitializer(elem_type, init, index);
            if (elem_str == "0") {
                array += elem_type->print() + " zeroinitializer , ";
            } else {
                array += elem_type->print() + " " + elem_str + ", ";
                all_zero = false;
            }
        }
        if (!all_zero)
            array.erase(array.size() - 2, 2);
        if (all_zero)
            return "0";
        else
            return "[" + array + "]";
    }
}

string GlobalVariable::print() const {
    string init_ir;
    auto elem_type = get_type()->as<PointerType>()->get_elem_type();
    if (elem_type->is_basic_type()) {
        return get_name() + " = global " + elem_type->print() + " " +
               get_init(0)->get_name();
    } else {
        auto iter = _init.begin();
        auto [_, str] = _gen_initializer(elem_type, 0, iter);
        return get_name() + " = global " + str;
    }
}
