#pragma once

#include "context.hh"
#include "instruction.hh"
#include "mir_builder.hh"
#include "mir_function.hh"
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
        std::map<mir::VirtualRegister *, mir::StatckObject *>
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
    void resolve_ret();
    void resolve_call();
    void resolve_stack();

    // to make sure function args in consistence with register allocation result
    // !! used after sp move in prologue
    void coordinate_func_args();

    void insert_inst(mir::MIR_INST op, std::vector<mir::Value *> vec);

    struct ArgInfo {
        struct _info {
            bool valid{false};
            bool is_float;
            // mir::Register::RegIDType vreg;
            std::variant<mir::Offset, mir::PhysicalRegister *> location;
        };
        std::array<_info, 8> int_args_in_reg;
        std::array<_info, 8> float_args_in_reg;
        std::vector<_info> args_in_stack;
    };
    // split function args to int/float, parse their location info
    ArgInfo split_func_args() const;

    // @brief: make sure immediate in range
    //
    // This function is for load/store like instructions:
    // lw/ld/.. rd, offset(sp)
    //
    // @label: If label is speicified, use `label->add_inst`, else
    // `insert_inst`
    // @temp_reg: If overflow on 12bit imm, use that temp_reg as address reg
    void safe_load_store(mir::MIR_INST, mir::PhysicalRegister *rd, int offset,
                         mir::IPReg *temp_reg, mir::Label *label = nullptr);

    // return a set containing physical regs which hold valid value,
    // relying on the data in `_upgrade_context`
    std::set<mir::Register::RegIDType>
    current_critical_regs(bool want_float) const;
};

}; // namespace codegen
