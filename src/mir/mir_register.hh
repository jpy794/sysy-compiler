#pragma once

#include "err.hh"
#include "mir_value.hh"
#include <array>
#include <cassert>
#include <memory>

namespace mir {

class Register : public Value {
  public:
    using RegIDType = unsigned;

  protected:
    RegIDType _id;
    explicit Register(RegIDType id) : _id(id) {}

  public:
    RegIDType get_id() const { return _id; }
};

class VirtualRegister : public Register {
  protected:
    VirtualRegister(RegIDType id) : Register(id) {}
};

/* int virtual register */
class IVReg final : public VirtualRegister {
    friend class ValueManager;
    static RegIDType TOTAL;

  private:
    IVReg() : VirtualRegister(++TOTAL) {}

  public:
    void dump(std::ostream &os, const Context &context) const final {
        os << "ireg" << std::to_string(_id);
    }
};

/* float virtual register */
class FVReg final : public VirtualRegister {
    friend class ValueManager;
    static RegIDType TOTAL;

  private:
    FVReg() : VirtualRegister(++TOTAL) {}

  public:
    void dump(std::ostream &os, const Context &context) const final {
        os << "freg" << std::to_string(_id);
    }
};

inline Register::RegIDType IVReg::TOTAL = 0;
inline Register::RegIDType FVReg::TOTAL = 0;

class PhysicalRegister : public Register {
  public:
    enum class Saver { None, Caller, Callee };

  protected:
    std::string _abi_name;
    PhysicalRegister(RegIDType id, std::string name)
        : Register(id), _abi_name(name) {}

  public:
    void dump(std::ostream &os, const Context &context) const override {
        os << _abi_name;
    }
    virtual Saver get_saver() const = 0;
};

class IPReg final : public PhysicalRegister {
    friend class PhysicalRegisterManager;
    IPReg(RegIDType id, std::string name) : PhysicalRegister(id, name) {}

  public:
    virtual Saver get_saver() const {
        if (_id == 0 or _id == 3 or _id == 4)
            return Saver::None;
        if (_id == 2 or _id == 8 or _id == 9 or (18 <= _id and _id >= 27))
            return Saver::Callee;
        return Saver::Caller;
    }
};

class FPReg final : public PhysicalRegister {
    friend class PhysicalRegisterManager;
    FPReg(RegIDType id, std::string name) : PhysicalRegister(id, name) {}

  public:
    virtual Saver get_saver() const {
        if (_id == 8 or _id == 9 or (18 <= _id and _id <= 27))
            return Saver::Callee;
        return Saver::Caller;
    }
};

class PhysicalRegisterManager {
  public:
    using RegPtr = PhysicalRegister *;
    using IPRegPtr = IPReg *;
    using ConstIPRegPtr = const IPReg *;
    using FPRegPtr = FPReg *;
    using ConstFPRegPtr = const FPReg *;

  private:
    std::array<IPReg, 32> _int_registers{
        IPReg{0, "zero"}, {1, "ra"},   {2, "sp"},   {3, "gp"},  {4, "tp"},
        {5, "t0"},        {6, "t1"},   {7, "t2"},   {8, "s0"},  {9, "s1"},
        {10, "a0"},       {11, "a1"},  {12, "a2"},  {13, "a3"}, {14, "a4"},
        {15, "a5"},       {16, "a6"},  {17, "a7"},  {18, "s2"}, {19, "s3"},
        {20, "s4"},       {21, "s5"},  {22, "s6"},  {23, "s7"}, {24, "s8"},
        {25, "s9"},       {26, "s10"}, {27, "s11"}, {28, "t3"}, {29, "t4"},
        {30, "t5"},       {31, "t6"},
    };
    std::array<FPReg, 32> _float_registers{
        FPReg{0, "ft0"}, {1, "ft1"},   {2, "ft2"},   {3, "ft3"},  {4, "ft4"},
        {5, "ft5"},      {6, "ft6"},   {7, "ft7"},   {8, "fs0"},  {9, "fs1"},
        {10, "fa0"},     {11, "fa1"},  {12, "fa2"},  {13, "fa3"}, {14, "fa4"},
        {15, "fa5"},     {16, "fa6"},  {17, "fa7"},  {18, "fs2"}, {19, "fs3"},
        {20, "fs4"},     {21, "fs5"},  {22, "fs6"},  {23, "fs7"}, {24, "fs8"},
        {25, "fs9"},     {26, "fs10"}, {27, "fs11"}, {28, "ft8"}, {29, "ft9"},
        {30, "ft10"},    {31, "ft11"}};

    PhysicalRegisterManager() = default;

  public:
    static PhysicalRegisterManager &get() {
        static PhysicalRegisterManager factory;
        return factory;
    }
    IPRegPtr zero() { return &_int_registers[0]; }
    IPRegPtr ra() { return &_int_registers[1]; }
    IPRegPtr sp() { return &_int_registers[2]; }
    IPRegPtr fp() { return &_int_registers[8]; }

    IPRegPtr temp(unsigned i) {
        assert(i <= 6);
        if (i <= 2)
            return &_int_registers[i + 5];
        else
            return &_int_registers[i + 25];
    }
    FPRegPtr ftemp(unsigned i) {
        assert(i <= 11);
        if (i <= 7)
            return &_float_registers[i];
        else
            return &_float_registers[i + 20];
    }

    IPRegPtr saved(unsigned i) {
        assert(i <= 11);
        if (i <= 1)
            return &_int_registers[i + 8];
        else
            return &_int_registers[i + 16];
    }
    FPRegPtr fsaved(unsigned i) {
        assert(i <= 11);
        if (i <= 1)
            return &_float_registers[i + 8];
        else
            return &_float_registers[i + 16];
    }

    IPRegPtr arg(unsigned i) {
        assert(i <= 7);
        return &_int_registers[i + 10];
    }
    FPRegPtr farg(unsigned i) {
        assert(i <= 7);
        return &_float_registers[i + 10];
    }

    IPRegPtr a(unsigned i) {
        assert(i <= 7);
        return &_int_registers[i + 10];
    }
    FPRegPtr fa(unsigned i) {
        assert(i <= 7);
        return &_float_registers[i + 10];
    }

    IPRegPtr ret_val_i(unsigned i) {
        assert(i <= 2);
        return &_int_registers[i + 10];
    }
    FPRegPtr ret_val_f(unsigned i) {
        assert(i <= 2);
        return &_float_registers[i + 10];
    }

    IPRegPtr get_int_reg(unsigned i) {
        assert(i <= 31);
        return &_int_registers[i];
    }
    FPRegPtr get_float_reg(unsigned i) {
        assert(i <= 31);
        return &_float_registers[i];
    }
    RegPtr get_reg(unsigned i, bool want_float) {
        if (want_float)
            return get_float_reg(i);
        else
            return get_int_reg(i);
    }
};

}; // namespace mir
