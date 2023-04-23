#pragma once

#include "Module.hh"

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
    Type(TypeID tid, Module* m);
    ~Type() = default;
    bool is_int_type() const { return tid_ == IntTypeID; }
    bool is_float_type() const { return tid_ == FloatTypeID; }
    bool is_void_type() const { return tid_ == VoidTypeID; }
    bool is_label_type() const { return tid_ == LabelTypeID; }
    bool is_function_type() const { return tid_ == FunctionTypeID; }
    bool is_pointer_type() const { return tid_ == PointerTypeID; }
    bool is_array_type() const { return tid_ == ArrayTypeID; }

private:
    TypeID tid_;
    Module* m_;
};

class IntType : Type {
private:
    unsigned nbits;

public:
    IntType(unsigned nbits, Module* m): Type(Int, )
};
class FloatType : Type {
};
class VoidType : Type {
};
class LabelType : Type {
};
class FuncType : Type {
};
class PointerType : Type {
};
class ArrayType : Type {
};
