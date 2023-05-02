#include "constant.hh"
#include "module.hh"

#include <memory>
#include <unordered_map>

using namespace ir;

ConstantInt *ConstantInt::get(int val, Module *m) {
    return m->get_const_int(val);
}
std::string ConstantInt::print() const { return this->get_name(); }
ConstantInt *ConstantInt::get(bool val, Module *m) {
    return m->get_const_bool(val);
}
ConstantFloat *ConstantFloat::get(float val, Module *m) {
    return m->get_const_float(val);
}
std::string ConstantFloat::print() const { return this->get_name(); }

ConstantArray *ConstantArray::get(std::vector<int> &array, Module *m) {
    std::vector<Constant *> array_;
    array_.reserve(array.size());
    for (auto val : array)
        array_.push_back(ConstantInt::get(val, m));
    return new ConstantArray(
        m->get_array_type(m->get_int32_type(), {(unsigned)array.size()}),
        array_);
}

ConstantArray *ConstantArray::get(std::vector<float> &array, Module *m) {
    std::vector<Constant *> array_;
    array_.reserve(array.size());
    for (auto val : array)
        array_.push_back(ConstantFloat::get(val, m));
    return new ConstantArray(
        m->get_array_type(m->get_float_type(), {(unsigned)array.size()}),
        array_);
}
std::string ConstantArray::print() const {
    std::string elems;
    for (auto &elem : this->_array) {
        elems += elem->get_type()->print() + " " + elem->print() + ", ";
    }
    elems.erase(elems.length() - 2, 2);
    return "[" + elems + "]";
}
