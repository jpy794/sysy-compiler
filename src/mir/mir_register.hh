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
  protected:
    std::string _abi_name;
    PhysicalRegister(RegIDType id, std::string name)
        : Register(id), _abi_name(name) {}

  public:
    void dump(std::ostream &os, const Context &context) const override {
        os << _abi_name;
    }
};

class IPReg final : public PhysicalRegister {
    friend class PhysicalRegisterManager;
    IPReg(RegIDType id, std::string name) : PhysicalRegister(id, name) {}
};

class FPReg final : public PhysicalRegister {
    friend class PhysicalRegisterManager;
    FPReg(RegIDType id, std::string name) : PhysicalRegister(id, name) {
        throw not_implemented_error{};
    }
};

class PhysicalRegisterManager {
  public:
    using IPRegPtr = std::shared_ptr<IPReg>;
    enum Saver { None, Caller, Callee };

  private:
    template <typename... Args> static IPRegPtr createIPReg(Args... args) {
        return IPRegPtr(new IPReg(args...));
    }

    std::array<IPRegPtr, 32> _int_registers{
        createIPReg(0, "zero"), createIPReg(1, "ra"),  createIPReg(2, "sp"),
        createIPReg(3, "gp"),   createIPReg(4, "tp"),  createIPReg(5, "t0"),
        createIPReg(6, "t1"),   createIPReg(7, "t2"),  createIPReg(8, "s0"),
        createIPReg(9, "s1"),   createIPReg(10, "a0"), createIPReg(11, "a1"),
        createIPReg(12, "a2"),  createIPReg(13, "a3"), createIPReg(14, "a4"),
        createIPReg(15, "a5"),  createIPReg(16, "a6"), createIPReg(17, "a7"),
        createIPReg(18, "s2"),  createIPReg(19, "s3"), createIPReg(20, "s4"),
        createIPReg(21, "s5"),  createIPReg(22, "s6"), createIPReg(23, "s7"),
        createIPReg(24, "s8"),  createIPReg(25, "s9"), createIPReg(26, "s10"),
        createIPReg(27, "s11"), createIPReg(28, "t3"), createIPReg(29, "t4"),
        createIPReg(30, "t5"),  createIPReg(31, "t6"),
    };
    // const std::array<FPReg *, 32> _float_registers{};

    PhysicalRegisterManager() = default;

  public:
    static PhysicalRegisterManager &get() {
        static PhysicalRegisterManager factory;
        return factory;
    }
    static Saver check_saver(const IPReg &reg) {
        auto id = reg._id;
        if (id == 0 or id == 3 or id == 4)
            return None;
        if (id == 2 or id == 8 or id == 9 or (18 <= id and id >= 27))
            return Callee;
        return Caller;
    }
    IPRegPtr zero() const { return _int_registers[0]; }
    IPRegPtr ra() const { return _int_registers[1]; }
    IPRegPtr sp() const { return _int_registers[2]; }

    IPRegPtr temp(unsigned i) const {
        assert(i <= 6);
        if (i <= 2)
            return _int_registers[i + 5];
        else
            return _int_registers[i + 25];
    }
    IPRegPtr saved(unsigned i) const {
        assert(i <= 11);
        if (i <= 1)
            return _int_registers[i + 8];
        else
            return _int_registers[i + 16];
    }

    IPRegPtr arg(unsigned i) const {
        assert(i <= 7);
        return _int_registers[i + 10];
    }

    IPRegPtr a(unsigned i) const {
        assert(i <= 7);
        return _int_registers[i + 10];
    }

    IPRegPtr ret_val_i(unsigned i) const {
        assert(i <= 2);
        return _int_registers[i + 10];
    }
};

}; // namespace mir
