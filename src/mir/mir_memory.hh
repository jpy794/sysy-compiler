#pragma once

#include "global_variable.hh"
#include "mir_config.hh"
#include "mir_instruction.hh"
#include "mir_register.hh"

#include <cassert>
#include <string>
#include <variant>
#include <vector>

namespace mir {

class MemObject : public Value {
  protected:
    const BasicType _type;
    const std::size_t _size;

    explicit MemObject(BasicType type, std::size_t size)
        : _type(type), _size(size) {
        assert(type != BasicType::VOID);
    }

  public:
    std::size_t get_size() const { return _size; }
    BasicType get_type() const { return _type; }
};

// local vars
class StackObject : public MemObject {
    friend class ValueManager;

  public:
    enum class Reason { Alloca, Spilled, CalleeSave };

  private:
    const std::size_t _align;
    Reason _reason;

  protected:
    StackObject(BasicType type, std::size_t size, std::size_t align,
                Reason reason)
        : MemObject(type, size), _align(align), _reason(reason) {}

  public:
    virtual void dump(std::ostream &os, const Context &context) const override;
    std::size_t get_align() const { return _align; }
    Reason get_reason() const { return _reason; }
    MIR_INST load_op() const { return mem_op(true); }
    MIR_INST store_op() const { return mem_op(false); }
    MIR_INST mem_op(bool load) const;
    bool is_float_usage() const;
};

class CalleeSave : public StackObject {
    friend class ValueManager;

  private:
    Register::RegIDType _regid;

  private:
    CalleeSave(BasicType type, Register::RegIDType id)
        : StackObject(
              type,
              type == BasicType::INT ? TARGET_MACHINE_SIZE : BASIC_TYPE_SIZE,
              type == BasicType::INT ? TARGET_MACHINE_SIZE : BASIC_TYPE_SIZE,
              Reason::CalleeSave),
          _regid(id) {}

  public:
    void dump(std::ostream &os, const Context &context) const override final;
    bool is_float_reg() const { return _type == BasicType::FLOAT; }
    Register::RegIDType saved_reg_id() const { return _regid; }
};

class GlobalObject : public MemObject {
    friend class ValueManager;

  private:
    std::string _name;
    InitPairs _inits;

  private:
    GlobalObject(const ir::GlobalVariable *global);

  public:
    void dump(std::ostream &os, const Context &context) const override final;
    const InitPairs &get_init() const { return _inits; }
};

}; // namespace mir
