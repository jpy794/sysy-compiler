#pragma once

#include <cstddef>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "err.hh"
#include "hash.hh"
#include "type.hh"
#include "utils.hh"
#include "value.hh"

namespace ir {

class Constant : public Value {
  public:
    Constant(Type *type, std::string &&name) : Value(type, std::move(name)){};
    virtual ~Constant() = default;
    std::string print() const final { throw unreachable_error{}; }

    template <typename Derived> bool is() { return ::is_a<Derived>(this); }
    template <typename Derived> Derived *as() { return ::as_a<Derived>(this); }
};

class ConstInt : public Constant {
  private:
    int _val;

  public:
    ConstInt(int val)
        : Constant(Types::get().int_type(), std::to_string(val)), _val(val){};

    int val() const { return _val; }
};

class ConstBool : public Constant {
  private:
    bool _val;

  public:
    ConstBool(bool val)
        : Constant(Types::get().bool_type(), val ? "true" : "false"),
          _val(val){};

    bool val() const { return _val; }
};

class ConstFloat : public Constant {
  private:
    float _val;
    std::string to_hex_str(float val) {
        double v = val;
        std::stringstream hex_s;
        hex_s << "0x" << std::hex << std::uppercase
              << *reinterpret_cast<uint64_t *>(&v);
        return hex_s.str();
    }

  public:
    ConstFloat(float val)
        : Constant(Types::get().float_type(), to_hex_str(val)), _val(val){};

    float val() const { return _val; }
};

class ConstArray : public Constant {
  private:
    std::vector<Constant *> _array;

    static Type *_deduce_type(const std::vector<Constant *> &array) {
        assert(array.size() > 0);
        auto elem_type = array[0]->get_type();
        for (auto i : array) {
            assert(i->get_type() == elem_type);
        }
        return Types::get().array_type(elem_type, array.size());
    }

    static std::string _gen_name(const std::vector<Constant *> &array) {
        std::string ret;
        ret += '[';
        for (size_t i = 0; i < array.size(); i++) {
            auto &elem = array[i];
            ret += elem->get_type()->print();
            ret += ' ';
            ret += elem->get_name();
            if (i != array.size() - 1) {
                ret += ", ";
            }
        }
        ret += ']';
        return ret;
    }

  public:
    // vals.size() > 0
    ConstArray(std::vector<Constant *> &&array)
        : Constant(_deduce_type(array), _gen_name(array)), _array(array){};

    std::vector<Constant *> &array() { return _array; }
};
class ConstZero : public Constant {
  private:
    static std::string _gen_name(Type *type) {
        if (!type->is_basic_type()) {
            return "zeroinitializer";
        } else if (type->is<IntType>()) {
            return "0";
        } else if (type->is<FloatType>()) {
            double v = 0;
            std::stringstream hex_s;
            hex_s << "0x" << std::hex << std::uppercase
                  << *reinterpret_cast<uint64_t *>(&v);
            return hex_s.str();
        } else {
            throw std::logic_error{"ConstZero type is " + type->print()};
        }
    }

  public:
    ConstZero(Type *type) : Constant(type, _gen_name(type)) {}
};
class Undef : public Constant {
  public:
    Undef(Type *type) : Constant(type, "undef") {}
};
// manage memory for consts, each const has to survive longer than its last use
// here for simplicity, just never delete consts
class Constants {
  private:
    // singleton
    Constants() = default;
    ~Constants() {
        for (auto &&[_, con] : _bool_hash) {
            delete con;
        }
        for (auto &&[_, con] : _int_hash) {
            delete con;
        }
        for (auto &&[_, con] : _float_hash) {
            delete con;
        }
        for (auto &&[_, con] : _array_hash) {
            delete con;
        }
        for (auto &&[_, con] : _zero_hash) {
            delete con;
        }
    }

    std::unordered_map<bool, ConstBool *> _bool_hash;
    std::unordered_map<int, ConstInt *> _int_hash;
    std::unordered_map<float, ConstFloat *> _float_hash;
    std::unordered_map<std::vector<Constant *>, ConstArray *, VectorHash>
        _array_hash;
    std::unordered_map<Type *, ConstZero *> _zero_hash;
    std::tuple<Undef *, Undef *, Undef *> _undef{
        new Undef(Types::get().bool_type()), new Undef(Types::get().int_type()),
        new Undef(Types::get().float_type())};

  public:
    static Constants &get() {
        static Constants factory;
        return factory;
    }

    ConstBool *bool_const(bool val) {
        if (not contains(_bool_hash, val)) {
            _bool_hash.insert({val, new ConstBool{val}});
        }
        return _bool_hash[val];
    }

    ConstInt *int_const(int val) {
        if (not contains(_int_hash, val)) {
            _int_hash.insert({val, new ConstInt{val}});
        }
        return _int_hash[val];
    }

    ConstFloat *float_const(float val) {
        if (not contains(_float_hash, val)) {
            _float_hash.insert({val, new ConstFloat{val}});
        }
        return _float_hash[val];
    }

    ConstArray *array_const(std::vector<Constant *> &&array) {
        if (not contains(_array_hash, array)) {
            _array_hash.insert({array, new ConstArray{std::move(array)}});
        }
        return _array_hash[array];
    }

    ConstZero *zero_const(Type *type) {
        if (not contains(_zero_hash, type)) {
            _zero_hash.insert({type, new ConstZero{type}});
        }
        return _zero_hash[type];
    }

    Undef *undef(Value *val) const {
        if (val->get_type()->is<BoolType>()) {
            return std::get<0>(_undef);
        } else if (val->get_type()->is<IntType>()) {
            return std::get<1>(_undef);
        } else if (val->get_type()->is<FloatType>()) {
            return std::get<2>(_undef);
        } else {
            throw std::logic_error{val->get_type()->print() +
                                   " can't be undef"};
        }
    }
};

} // namespace ir
