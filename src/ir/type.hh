#pragma once

#include "err.hh"
#include "hash.hh"
#include "utils.hh"

#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ir {

/* Design Rule:
 * - maintain enough class members to express the semantics
 * - use api to access the class members
 * - implement some api to simplify caller's logic
 * - pursue safety & robustness: use const, assert etc to exit early when error
 * occurs
 * - add print() api for IR generation
 */

class Type {
  public:
    virtual ~Type() = default;
    virtual std::string print() const = 0;

    template <typename Derived> bool is() { return is_a<Derived>(this); }
    template <typename Derived> Derived *as() { return as_a<Derived>(this); }

    bool is_basic_type();
    bool is_legal_ret_type();
    bool is_legal_param_type();
};

class FloatType : public Type {
  public:
    std::string print() const final { return "float"; }
};

class VoidType : public Type {
  public:
    std::string print() const final { return "void"; }
};

class BoolType : public Type {
  public:
    std::string print() const final { return "i1"; }
};

class IntType : public Type {
  public:
    virtual std::string print() const { return "i32"; }
};

class I64IntType : public Type {
  public:
    virtual std::string print() const { return "i64"; }
};

class PointerType : public Type {
  private:
    Type *const _elementTp;

  public:
    PointerType(Type *elementTp) : _elementTp(elementTp) {}
    Type *get_elem_type() const { return _elementTp; }
    std::string print() const final { return _elementTp->print() + '*'; }
};

class ArrayType : public Type {
  private:
    Type *const _elem_type;  // base_type or sub array
    const size_t _elem_cnt;  // the outer dim
    Type *const _base_type;  // int|float
    const size_t _total_cnt; // how many int/float does it contain
    size_t _dims;

  public:
    ArrayType(Type *elem_type, size_t elem_cnt)
        : _elem_type(elem_type), _elem_cnt(elem_cnt),
          _base_type(_get_base_type()), _total_cnt(_get_total_cnt()) {
        assert(elem_cnt > 0);
        if (elem_type->is<ArrayType>())
            _dims = elem_type->as<ArrayType>()->get_dims() + 1;
        else
            _dims = 1;
    }

    Type *get_base_type() const { return _base_type; }
    Type *get_elem_type() const { return _elem_type; }
    size_t get_elem_cnt() const { return _elem_cnt; }
    size_t get_total_cnt() const { return _total_cnt; }
    size_t get_dims() const { return _dims; }

    std::string print() const final {
        return '[' + std::to_string(_elem_cnt) + " x " + _elem_type->print() +
               ']';
    }

  private:
    Type *_get_base_type() const;
    size_t _get_total_cnt() const;
};

class LabelType : public Type {
    std::string print() const final {
        // we do not need this for now
        throw unreachable_error{};

        return "label";
    }
};

class FuncType : public Type {
  private:
    Type *const _ret_type;
    const std::vector<Type *> _param_types;

  public:
    FuncType(Type *ret_type, const std::vector<Type *> &&param_types)
        : _ret_type(ret_type), _param_types(param_types) {}

    Type *get_result_type() const { return _ret_type; }
    Type *get_param_type(unsigned i) const { return _param_types.at(i); }

    const decltype(_param_types) &get_param_types() const {
        return _param_types;
    }

    // we do not need this for now
    std::string print() const final { throw unreachable_error{}; }
};

// give each type a unique address, for convenience of equal-judge
// manage memory for types, each type has to survive longer than its last use
// here for simplicity, just never delete type
class Types {
  private:
    // singleton
    Types() = default;
    ~Types() {
        for (auto &&[_, type] : _arr_ptr_hash) {
            delete type;
        }
        for (auto &&[_, type] : _func_hash) {
            delete type;
        }
        delete _bool_tp;
        delete _int_tp;
        delete _float_tp;
        delete _void_tp;
        delete _label_tp;
    }

    BoolType *_bool_tp{new BoolType};
    IntType *_int_tp{new IntType};
    I64IntType *_i64_int_tp{new I64IntType};
    FloatType *_float_tp{new FloatType};
    VoidType *_void_tp{new VoidType};
    LabelType *_label_tp{new LabelType};

    // hash map
    std::unordered_map<std::pair<Type *, size_t>, Type *, PairHash>
        _arr_ptr_hash;
    std::unordered_map<std::pair<Type *, std::vector<Type *>>, FuncType *,
                       PairHash>
        _func_hash;

    Type *_arr_or_ptr_type(Type *elem_type, size_t elem_cnt) {
        auto key = std::pair{elem_type, elem_cnt};
        if (not contains(_arr_ptr_hash, key)) {
            Type *val{nullptr};
            if (elem_cnt != 0) {
                // arr
                val = new ArrayType{elem_type, elem_cnt};
            } else {
                // ptr
                val = new PointerType{elem_type};
            }
            _arr_ptr_hash.insert({key, val});
        }
        return _arr_ptr_hash[key];
    }

  public:
    static Types &get() {
        static Types factory;
        return factory;
    }

    BoolType *bool_type() const { return _bool_tp; }
    IntType *int_type() const { return _int_tp; }
    FloatType *float_type() const { return _float_tp; }
    VoidType *void_type() const { return _void_tp; }
    LabelType *label_type() const { return _label_tp; }
    I64IntType *i64_int_type() const { return _i64_int_tp; }
    PointerType *ptr_type(Type *elem_type) {
        return as_a<PointerType>(_arr_or_ptr_type(elem_type, 0));
    }

    ArrayType *array_type(Type *elem_type, size_t elem_cnt) {
        assert(elem_cnt > 0);
        return as_a<ArrayType>(_arr_or_ptr_type(elem_type, elem_cnt));
    }

    FuncType *func_type(Type *ret_type, std::vector<Type *> &&param_types) {
        auto key = std::pair{ret_type, param_types};
        if (not contains(_func_hash, key)) {
            auto val = new FuncType{ret_type, std::move(param_types)};
            _func_hash.insert({key, val});
        }
        return _func_hash[key];
    }
};

} // namespace ir
