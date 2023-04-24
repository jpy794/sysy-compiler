#pragma once
class Constant: public Value{
    public:
        Constant(Type* type, const std::string &name) : Value(type, name) {};
        ~Constant()=default;
        // virtual operator<(const Constant& lhs, const Constant& rhs);
};
class ConstantInt: public Constant{
    public:
        ConstantInt(Type* type, int val) : Constant(type, std::to_string(val)), value_(val) {};
        ~ConstantInt()=default;
        int get_value() const { return value_; }
        static ConstantInt* get(int val, Module* m);
    private:
        int value_;
};
class ConstantFloat: public Constant{
    public:
        ConstantFloat(Type* type, float val) : Constant(type, std::to_string(val)), value_(val) {};
        ~ConstantFloat()=default;
        float get_value() const { return value_; }
        static ConstantFloat* get(float val, Module* m);
    private:
        float value_;
};
class ConstantArray: public Constant{
    public:
        ConstantArray(Type* type, std::vector<Constant*> &array) : Constant(type, ""), array_(array) {};
        ~ConstantArray()=default;
        static ConstantArray* get(std::vector<int> &array, Module* m);
        static ConstantArray* get(std::vector<float> &array, Module* m);
    private:
        std::vector<Constant*> array_;
};