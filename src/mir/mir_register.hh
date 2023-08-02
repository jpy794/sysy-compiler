#pragma once

#include "err.hh"
#include "mir_value.hh"
#include <array>
#include <cassert>
#include <memory>
#include <set>

namespace mir {

class Register : public Value {
  public:
    using RegIDType = unsigned;

  protected:
    RegIDType _id;
    explicit Register(RegIDType id) : _id(id) {}

  public:
    RegIDType get_id() const { return _id; }
    bool is_int_register() const;
    bool is_float_register() const;
    void assert_int() const;
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

inline Register::RegIDType IVReg::TOTAL = 32;
inline Register::RegIDType FVReg::TOTAL = 32;

class PhysicalRegister : public Register {
  public:
    class Saver {
      public:
        enum SaverType { None, Caller, Callee, ALL };

        Saver(SaverType mode) : _mode(mode) {}
        bool operator==(const Saver &other) {
            if (this->_mode == other._mode)
                return true;
            if (this->_mode == None or other._mode == None)
                return false;
            if (this->_mode == ALL or other._mode == ALL)
                return true;
            return false;
        }

      private:
        SaverType _mode;
    };

  protected:
    std::string _abi_name;
    PhysicalRegister(RegIDType id, std::string name)
        : Register(id), _abi_name(name) {}

  public:
    void dump(std::ostream &os, const Context &context) const override {
        os << _abi_name;
    }
    virtual Saver get_saver() const = 0;
    virtual bool is_arg_reg() const = 0;
    virtual bool is_temp_reg() const = 0;
    virtual unsigned get_arg_idx() const = 0;
};

class IPReg final : public PhysicalRegister {
    friend class PhysicalRegisterManager;
    IPReg(RegIDType id, std::string name) : PhysicalRegister(id, name) {}

  public:
    virtual Saver get_saver() const {
        if (_id == 1)
            return Saver::ALL;
        if (_id == 0 or _id == 3 or _id == 4)
            return Saver::None;
        if (_id == 2 or _id == 8 or _id == 9 or (18 <= _id and _id <= 27))
            return Saver::Callee;
        return Saver::Caller;
    }
    virtual bool is_arg_reg() const { return 10 <= _id and _id <= 17; }
    virtual bool is_temp_reg() const {
        return (5 <= _id and _id <= 7) or (28 <= _id and _id <= 31);
    }
    virtual unsigned get_arg_idx() const {
        assert(is_arg_reg());
        return _id - 10;
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
    virtual bool is_arg_reg() const { return 10 <= _id and _id <= 17; }
    virtual bool is_temp_reg() const {
        return (0 <= _id and _id <= 7) or (28 <= _id and _id <= 31);
    }
    virtual unsigned get_arg_idx() const {
        assert(is_arg_reg());
        return _id - 10;
    }
};

class PhysicalRegisterManager {
  public:
    using RegPtr = PhysicalRegister *;
    using ConstRegPtr = PhysicalRegister *;
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

    // get registers which is seen as t{}/ft{}
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
    RegPtr get_temp_reg(unsigned i, bool want_float) {
        if (want_float)
            return ftemp(i);
        else
            return temp(i);
    }

    // get registers which is seen as s{}/fs{}
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

    // get registers which is used as argument
    IPRegPtr arg(unsigned i) {
        assert(i <= 7);
        return &_int_registers[i + 10];
    }
    FPRegPtr farg(unsigned i) {
        assert(i <= 7);
        return &_float_registers[i + 10];
    }
    RegPtr get_arg_reg(unsigned i, bool want_float) {
        if (want_float)
            return farg(i);
        else
            return arg(i);
    }

    // get registers which is seen as a{}/fa{}
    IPRegPtr a(unsigned i) {
        assert(i <= 7);
        return &_int_registers[i + 10];
    }
    FPRegPtr fa(unsigned i) {
        assert(i <= 7);
        return &_float_registers[i + 10];
    }

    // get registers which is used as return value
    IPRegPtr ret_val_i(unsigned i) {
        assert(i <= 2);
        return &_int_registers[i + 10];
    }
    FPRegPtr ret_val_f(unsigned i) {
        assert(i <= 2);
        return &_float_registers[i + 10];
    }
    RegPtr get_ret_val_reg(unsigned i, bool want_float) {
        if (want_float)
            return ret_val_f(i);
        else
            return ret_val_i(i);
    }

    // get reg through basic idx
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

    // get all registers able to write
    const std::set<RegPtr> &get_all_regs_writable(bool want_float) {
        static std::set<RegPtr> floats, ints;
        static bool run_once = false;
        if (not run_once) {
            run_once = true;
            for (unsigned i = 5; i < 32; ++i)
                ints.insert(&_int_registers[i]);
            for (auto &freg : _float_registers)
                floats.insert(&freg);
        }
        return want_float ? floats : ints;
    }
};

}; // namespace mir
