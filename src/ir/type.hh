#pragma once

#include "module.hh"

#include <cassert>
#include <string>
#include <vector>

class Module;
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
    enum TypeID {
        IntTypeID,
        FloatTypeID,
        VoidTypeID,
        LabelTypeID,
        FunctionTypeID,
        PointerTypeID,
        ArrayTypeID
    };
    explicit Type(TypeID tid, Module *m);
    // ~Type() = default;

    bool is_int_type() const { return _tid == IntTypeID; }
    bool is_float_type() const { return _tid == FloatTypeID; }
    bool is_void_type() const { return _tid == VoidTypeID; }
    bool is_label_type() const { return _tid == LabelTypeID; }
    bool is_function_type() const { return _tid == FunctionTypeID; }
    bool is_pointer_type() const { return _tid == PointerTypeID; }
    bool is_array_type() const { return _tid == ArrayTypeID; }

    Module *get_module() const { return _m; }

    virtual std::string print() const;

  private:
    TypeID _tid;
    Module *_m;
};

class FloatType : public Type {
  public:
    FloatType(Module *m) : Type(FloatTypeID, m) {}
    std::string print() const { return "float"; }
};

class VoidType : public Type {
  public:
    VoidType(Module *m) : Type(VoidTypeID, m) {}
    std::string print() const { return "void"; }
};

class FuncType : public Type {
  private:
    Type *_retTp;
    std::vector<Type *> _paramsTp;

  public:
    FuncType(Module *m, Type *ret, const std::vector<Type *> &params)
        : Type(FunctionTypeID, m), _retTp(ret), _paramsTp(params) {}

    unsigned get_num_params() const { return _paramsTp.size(); }

    Type *get_result_type() const { return _retTp; }
    Type *get_param_type(unsigned i) const { return _paramsTp.at(i); }

    const decltype(_paramsTp) &get_params() const { return _paramsTp; }

    static bool is_valid_return_type(Type *);
    // Is ArrayType able to be passed in?
    // Answer maybe: An array is passed in by start address
    static bool is_valid_argument_type(Type *);

    std::string print() const; // Is this useful?
};

class IntType : public Type {
  private:
    unsigned _nbits;

  public:
    IntType(Module *m, unsigned nbits) : Type(IntTypeID, m), _nbits(nbits) {
        assert((_nbits == 1 or _nbits == 32) &&
               "we don't support other int type");
    };

    unsigned get_num_bits() const { return _nbits; }
    std::string print() const { return "i" + std::to_string(_nbits); }
};

class PointerType : public Type {
  private:
    Type *_elementTp;

  public:
    PointerType(Module *m, Type *elementTp)
        : Type(PointerTypeID, m), _elementTp(elementTp) {}
    Type *get_element_type() const { return _elementTp; }
    std::string print() const { return _elementTp->print() + "*"; }
};

class LabelType : public Type {
  public:
    LabelType(Module *m) : Type(LabelTypeID, m) {}
    std::string print() const { return "SysYlabel"; }
};
class ArrayType : public Type {
  private:
    Type *_elementTp;
    unsigned _length;
    // std::vector<unsigned> _dims;

  public:
    // ArrayType(Module *m, Type *elem_type, std::vector<unsigned> dims)
    //     : Type(ArrayTypeID, m), _elementTp(elem_type), _dims(dims) {}
    ArrayType(Module *m, Type *elem_type, unsigned length)
        : Type(ArrayTypeID, m), _elementTp(elem_type), _length(length) {}

    Type *get_element_type() const { return _elementTp; }
    
    unsigned get_length() const { return _length; }

    // unsigned get_dim(unsigned i) const { return _dims.at(i); }

    // const decltype(_dims) &get_dims() const { return _dims; }

    std::string print() const;
};