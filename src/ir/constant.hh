#pragma once
#include <string>
#include <vector>

#include "module.hh"
#include "value.hh"
class Type;
class Module;
class Constant: public Value{
    public:
        Constant(Type* type, const std::string &name) : Value(type, name) {};
        ~Constant()=default;
        // virtual operator<(const Constant& lhs, const Constant& rhs);
};
class ConstantInt: public Constant{
    public:
        ConstantInt(Type* type, int val) : Constant(type, std::to_string(val)), _value(val) {};
        ~ConstantInt()=default;
        int get_value() const { return _value; }
        static ConstantInt* get(int val, Module* m);
    private:
        int _value;
};
class ConstantFloat: public Constant{
    public:
        ConstantFloat(Type* type, float val) : Constant(type, std::to_string(val)), _value(val) {};
        ~ConstantFloat()=default;
        float get_value() const { return _value; }
        static ConstantFloat* get(float val, Module* m);
    private:
        float _value;
};
class ConstantArray: public Constant{
    public:
        ConstantArray(Type* type, std::vector<Constant*> &array) : Constant(type, "array"), _array(array) {};
        ~ConstantArray()=default;
        static ConstantArray* get(std::vector<int> &array, Module* m);
        static ConstantArray* get(std::vector<float> &array, Module* m);
    private:
        std::vector<Constant*> _array;
};