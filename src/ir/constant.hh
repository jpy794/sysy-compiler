#pragma once

#include <string>
#include <vector>

#include "instruction.hh"
#include "value.hh"

namespace ir {

class Module;
class Constant : public Value {
  public:
    // FIXME: set to correct module
    Constant(Type *type, std::string &&name) : Value(nullptr, type, std::move(name)){};
    ~Constant() = default;
    // virtual operator<(const Constant& lhs, const Constant& rhs);
};
class ConstantInt : public Constant {
  public:
    ConstantInt(Type *type, int val)
        : Constant(type, std::to_string(val)), _value(val){};
    ~ConstantInt() = default;
    int get_value() const { return _value; }
    static ConstantInt *get(int val, Module *m);
    static ConstantInt *get(bool val, Module *m);
    std::string print() const override;

  private:
    int _value;
};
class ConstantFloat : public Constant {
  public:
    ConstantFloat(Type *type, float val)
        : Constant(type, std::to_string(val)), _value(val){};
    ~ConstantFloat() = default;
    float get_value() const { return _value; }
    static ConstantFloat *get(float val, Module *m);
    std::string print() const override;

  private:
    float _value;
};
class ConstantArray : public Constant {
  public:
    ConstantArray(Type *type, std::vector<Constant *> &array)
        : Constant(type, "array"), _array(array){};
    ~ConstantArray() = default;
    static ConstantArray *get(std::vector<int> &array, Module *m);
    static ConstantArray *get(std::vector<float> &array, Module *m);
    std::string print() const override;

  private:
    std::vector<Constant *> _array;
};

} // namespace ir
