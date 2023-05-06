#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "err.hh"
#include "type.hh"
#include "utils.hh"
#include "value.hh"

namespace ir {

class Constant : public Value {
  public:
    Constant(Type *type, std::string &&name) : Value(type, std::move(name)){};
    std::string print() const final { throw unreachable_error{}; }
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
        : Constant(Types::get().bool_type(),
                   std::to_string(static_cast<int>(val))),
          _val(val){};

    bool val() const { return _val; }
};

class ConstFloat : public Constant {
  private:
    float _val;

  public:
    ConstFloat(float val)
        : Constant(Types::get().float_type(), std::to_string(val)), _val(val){};

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
    }

    std::unordered_map<bool, Constant *> _bool_hash;
    std::unordered_map<int, Constant *> _int_hash;
    std::unordered_map<float, Constant *> _float_hash;
    std::unordered_map<std::vector<Constant *>, Constant *, VectorHash>
        _array_hash;

  public:
    static Constants &get() {
        static Constants factory;
        return factory;
    }

    Constant *bool_const(bool val) {
        if (not contains(_bool_hash, val)) {
            _bool_hash.insert({val, new ConstBool{val}});
        }
        return _bool_hash[val];
    }

    Constant *int_const(int val) {
        if (not contains(_int_hash, val)) {
            _int_hash.insert({val, new ConstInt{val}});
        }
        return _int_hash[val];
    }

    Constant *float_const(float val) {
        if (not contains(_float_hash, val)) {
            _float_hash.insert({val, new ConstFloat{val}});
        }
        return _float_hash[val];
    }

    Constant *array_const(std::vector<Constant *> &&array) {
        if (not contains(_array_hash, array)) {
            _array_hash.insert({array, new ConstArray{std::move(array)}});
        }
        return _array_hash[array];
    }
};

} // namespace ir
