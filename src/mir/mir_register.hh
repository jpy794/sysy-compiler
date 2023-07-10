#pragma once

#include "err.hh"
#include "mir_value.hh"

namespace mir {

class Register : public Value {
  protected:
    unsigned _id;
    explicit Register(unsigned id) : _id(id) {}

  public:
    unsigned get_id() const { return _id; }
};

class VirtualRegister : public Register {
  protected:
    VirtualRegister(unsigned id) : Register(id) {}
};

class PhysicalRegister : public Register {
  protected:
    PhysicalRegister(unsigned id) : Register(id) {
        throw not_implemented_error();
    }
};

/* int virtual register */
class IVReg : public VirtualRegister {
    friend class ValueManager;
    static unsigned TOTAL;

  protected:
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
class FVReg : public VirtualRegister {
    friend class ValueManager;
    static unsigned TOTAL;

  protected:
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

inline unsigned IVReg::TOTAL = 0;
inline unsigned FVReg::TOTAL = 0;

}; // namespace mir
