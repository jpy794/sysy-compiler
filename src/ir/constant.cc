#include "constant.hh"

#include <memory>
#include <unordered_map>

using namespace ir;

struct pair_hash {
    template <typename T>
    std::size_t operator()(const std::pair<T, Module *> val) const {
        auto lhs = std::hash<T>()(val.first);
        auto rhs =
            std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(val.second));
        return lhs ^ rhs;
    }
};

static std::unordered_map<std::pair<int, Module *>,
                          std::unique_ptr<ConstantInt>, pair_hash>
    cached_int;

static std::unordered_map<std::pair<float, Module *>,
                          std::unique_ptr<ConstantFloat>, pair_hash>
    cached_float;

ConstantInt *ConstantInt::get(int val, Module *m) {
    if (cached_int.find(std::make_pair(val, m)) != cached_int.end())
        return cached_int[std::make_pair(val, m)].get();
    else
        return (cached_int[std::make_pair(val, m)] =
                    std::unique_ptr<ConstantInt>(
                        new ConstantInt(m->get_int_type(), val)))
            .get();
}
std::string ConstantInt::print() const { return this->get_name(); }

ConstantFloat *ConstantFloat::get(float val, Module *m) {
    if (cached_float.find(std::make_pair(val, m)) != cached_float.end())
        return cached_float[std::make_pair(val, m)].get();
    else
        return (cached_float[std::make_pair(val, m)] =
                    std::unique_ptr<ConstantFloat>(
                        new ConstantFloat(m->get_float_type(), val)))
            .get();
}
std::string ConstantFloat::print() const { return this->get_name(); }

ConstantArray *ConstantArray::get(std::vector<int> &array, Module *m) {
    std::vector<Constant *> array_;
    array_.reserve(array.size());
    for (auto &val : array)
        array_.push_back(ConstantInt::get(val, m));
    return new ConstantArray(
        m->get_array_type(m->get_int_type(), (unsigned)array.size()), array_);
}

ConstantArray *ConstantArray::get(std::vector<float> &array, Module *m) {
    std::vector<Constant *> array_;
    array_.reserve(array.size());
    for (auto &val : array)
        array_.push_back(ConstantFloat::get(val, m));
    return new ConstantArray(
        m->get_array_type(m->get_float_type(), (unsigned)array.size()), array_);
}
std::string ConstantArray::print() const {
    std::string elems;
    for (auto &elem : this->_array) {
        elems += elem->print() + ",";
    }
    elems[elems.size() - 1] = '}';
    return "{" + elems;
}
