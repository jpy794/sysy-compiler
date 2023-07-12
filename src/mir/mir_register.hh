#pragma once

#include "err.hh"
#include "mir_value.hh"

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
    VirtualRegister(Register::RegIDType id) : Register(id) {}
};

class PhysicalRegister : public Register {
  protected:
    PhysicalRegister(Register::RegIDType id) : Register(id) {
        throw not_implemented_error();
    }
};

/* int virtual register */
class IVReg final : public VirtualRegister {
    friend class ValueManager;
    static Register::RegIDType TOTAL;

  private:
    IVReg() : VirtualRegister(++TOTAL) {}

  public:
    void dump(std::ostream &os, const MIRContext &context) const final {
        switch (context.stage) {
        case Stage::stage1:
            os << "ireg" << std::to_string(_id);
            break;
        case Stage::stage2:
            throw not_implemented_error{};
        }
    }
};

/* float virtual register */
class FVReg final : public VirtualRegister {
    friend class ValueManager;
    static Register::RegIDType TOTAL;

  private:
    FVReg() : VirtualRegister(++TOTAL) {}

  public:
    void dump(std::ostream &os, const MIRContext &context) const final {
        switch (context.stage) {
        case Stage::stage1:
            os << "freg" << std::to_string(_id);
            break;
        case Stage::stage2:
            throw not_implemented_error{};
        }
    }
};

inline Register::RegIDType IVReg::TOTAL = 0;
inline Register::RegIDType FVReg::TOTAL = 0;

}; // namespace mir
