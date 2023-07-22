#pragma once

#include "context.hh"
#include "instruction.hh"
#include "mir_builder.hh"
#include "mir_function.hh"
#include "mir_immediate.hh"
#include "mir_instruction.hh"
#include "mir_label.hh"
#include "mir_memory.hh"
#include "mir_module.hh"
#include "mir_register.hh"
#include "module.hh"
#include "regalloc.hh"

#include <array>
#include <bitset>
#include <cassert>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace codegen {

class CodeGen {
  private:
    context::Stage _stage{context::Stage::stage1};
    std::unique_ptr<mir::Module> _mir_module{nullptr};

    RegAlloc _allocator;

    struct {
        mir::Function *func{nullptr};
        mir::Label *label{nullptr};
        mir::Instruction *inst{nullptr};
        mir::Label *entry{nullptr}, *exit{nullptr}; // new created
        std::set<mir::Label *> new_labels{}; // should ignore during traverse
        std::map<mir::VirtualRegister *, mir::StackObject *>
            arg_spilled_location;

        void new_func(mir::Function *f) {
            func = f;
            label = entry = exit = nullptr;
            inst = nullptr;
            new_labels.clear();
            arg_spilled_location.clear();
        }
    } _upgrade_context;

  public:
    CodeGen(std::unique_ptr<ir::Module> &&ir_module) {
        mir::MIRBuilder builder(std::move(ir_module));
        _mir_module.reset(builder.release());
        _allocator.run(_mir_module.get());
        upgrade();
    }

    friend std::ostream &operator<<(std::ostream &os, const CodeGen &c);

  private:
    void construct_mir(ir::Module *);

    /* For each function, we do the upgrade in 2 steps.
     *
     * 1st step: eliminate all virtual registers.
     * If a virtual register:
     * - correspond to some physical one, just replace it
     * - is spilled, substitute to a StatckObject
     * During the first pass, we can get a complete info about frame info,
     * which contains: local vars(derived from stage1), spilled regs and
     * callee saves
     *
     * 2nd step: expand the uncompleted structions
     * - load stack object into a regsiter and replace the inst op
     * - call: pass arguments to function
     * - ret: make a0/fa0 and jump to a final return label
     */
    void upgrade();
    void upgrade_step1();
    void upgrade_step2();

    void resolve_logue();
    void resolve_stack();
    void resolve_ret();
    void resolve_call();

    // to make sure function args in consistence with register allocation result
    // !! used after sp move in prologue
    void coordinate_func_args();

    struct ArgInfo {
        struct _info {
            bool valid{false};
            bool is_float;
            // the immediate is just a workaround for call pass arg case
            std::variant<mir::Offset, mir::PhysicalRegister *, mir::Immediate *>
                location;
        };
        std::array<_info, 8> int_args_in_reg;
        std::array<_info, 8> float_args_in_reg;
        std::vector<_info> args_in_stack;
    };
    // split function args to int/float, parse their location info
    ArgInfo split_func_args_logue_ver() const; // used during resolve logue
    ArgInfo split_func_args_call_ver() const;  // used during expand call

    struct StackPassResult {
        mir::Offset stack_grow_size2;
        std::map<mir::PhysicalRegister *, bool> changed;
    };

    // used for call inst, pass arguments
    // after pass_args_stack, tmp regs backup:
    // - t0: -8(sp)
    // - t1: -16(sp)
    // - ft0: -20(sp)
    // @return: stack_grow_size2, that is grow size during pass on stack
    StackPassResult pass_args_stack(const ArgInfo &,
                                    mir::Offset stack_grow_size1);
    void pass_args_in_reg(const ArgInfo &, mir::Offset stack_grow_size1,
                          StackPassResult res, bool for_float);

    /* @brief: make sure immediate in range
     *
     * This function is for load/store like instructions:
     * lw/ld/.. rd, offset(sp)
     *
     * @label: If label is speicified, use `label->add_inst`, else
     * `insert_inst`
     * @tmp_reg: If overflow on 12bit imm, use that tmp_reg as address reg
     * @return: If the tmp_reg is overwriten
     */
    bool safe_load_store(mir::MIR_INST, mir::PhysicalRegister *rd_or_rs,
                         int offset, mir::IPReg *tmp_reg,
                         mir::Label *label = nullptr);
    bool stack_change(int delta, mir::IPReg *tmp_reg,
                      mir::Label *label = nullptr);

    mir::Instruction *insert_inst(mir::MIR_INST op,
                                  std::vector<mir::Value *> vec);
    mir::Instruction *gen_inst(mir::MIR_INST _op,
                               std::vector<mir::Value *> operands,
                               mir::Label *label);
    mir::Instruction *move_same_type(mir::Value *dest, mir::Value *src,
                                     mir::Label *label = nullptr);
    mir::Instruction *comment(std::string &&s, mir::Label *label = nullptr);

    // return a set containing physical regs which hold valid value,
    // relying on the data in `_upgrade_context`
    std::set<mir::Register::RegIDType>
    current_critical_regs(bool want_float,
                          mir::PhysicalRegister::Saver s =
                              mir::PhysicalRegister::Saver::None) const;
};

}; // namespace codegen
