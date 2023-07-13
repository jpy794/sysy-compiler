#pragma once

#include "err.hh"
#include "mir_value.hh"
#include <cassert>

namespace mir {

class Immediate : public Value {
  protected:
    int _value;

    Immediate(int v) : _value(v) {}

  public:
    int get_imm() { return _value; }
    void dump(std::ostream &os, const Context &context) const { os << _value; }
};

class Imm32bit : public Immediate {
    friend class ValueManager;
    Imm32bit(int v) : Immediate(v) {}
};

class Imm12bit : public Immediate {
    friend class ValueManager;
    Imm12bit(int v) : Immediate(v) { assert(check_in_range(v)); }

  public:
    static const int IMM12bitMIN;
    static const int IMM12bitMAX;
    static bool check_in_range(int imm) {
        return IMM12bitMIN <= imm and imm <= IMM12bitMAX;
    }
};

inline const int Imm12bit::IMM12bitMIN = -(1 << 11);
inline const int Imm12bit::IMM12bitMAX = (1 << 11) - 1;

}; // namespace mir
