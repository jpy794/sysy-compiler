#include "global_variable.hh"
#include "constant.hh"
#include "module.hh"
#include "type.hh"
#include <cstddef>
#include <stdexcept>

using namespace ir;

GlobalVariable::GlobalVariable(Type *type, Constant *init, std::string &&name)
    : Value(Types::get().ptr_type(type), "@" + name), _init(init){};

std::string GlobalVariable::_gen_zeroinitializer(Type *type, ConstArray *init,
                                                 size_t &index) {
    if (type->is_basic_type()) {
        return init->array()[index++]->get_name();
    } else {
        size_t elem_cnt = type->as<ArrayType>()->get_elem_cnt();
        auto elem_type = type->as<ArrayType>()->get_elem_type();
        std::string array;
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

std::string GlobalVariable::print() const {
    std::string init_ir;
    if (_init) {
        init_ir = " " + _init->get_name();
    } else {
        throw std::logic_error{"GlobalVariable doesn't have init"};
    }
    auto elem_type = get_type()->as<PointerType>()->get_elem_type();
    if (elem_type->is_basic_type()) {
        return get_name() + " = global " + elem_type->print() + " " +
               _init->get_name();
    } else {
        size_t index = 0;
        auto zero_init =
            _gen_zeroinitializer(elem_type, _init->as<ConstArray>(), index);
        if (zero_init == "0")
            return get_name() + " = global " + elem_type->print() + " " +
                   "zeroinitializer";
        else
            return get_name() + " = global " + elem_type->print() + " " +
                   zero_init;
    }
}
