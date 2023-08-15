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
#include "mir_value.hh"
#include "module.hh"
#include "peephole.hh"
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
        std::list<mir::Instruction *> multi_calls;

        void new_func(mir::Function *f) {
            func = f;
            label = entry = exit = nullptr;
            inst = nullptr;
            new_labels.clear();
            arg_spilled_location.clear();
            multi_calls.clear();
        }
    } _upgrade_context;

  public:
    CodeGen(std::unique_ptr<ir::Module> &&ir_module, bool optimize,
            bool stage1_only = false) {
        mir::MIRBuilder builder(std::move(ir_module));
        _mir_module.reset(builder.release());
        _allocator.run(_mir_module.get());
        if (not stage1_only) {
            upgrade();
            if (optimize) {
                PeepholeOpt opt(*_mir_module);
                opt.run();
            }
        }
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
    void resolve_move();

    // to make sure function args in consistence with register allocation result
    // !! used after sp move in prologue
    void coordinate_func_args();

    struct ArgInfo {
        struct _info {
            bool valid{false};
            bool is_float;
            // the immediate is just a workaround for call pass arg case
            std::variant<mir::StackObject *, mir::PhysicalRegister *,
                         mir::Immediate *>
                location;
        };
        std::array<_info, 8> int_args_in_reg;
        std::array<_info, 8> float_args_in_reg;
        std::vector<_info> args_in_stack;
    };
    /* split function args to int/float, parse their location info */
    // used during resolve logue
    ArgInfo split_func_args_logue_ver() const;
    // used during expand call
    ArgInfo split_func_args_call_ver(mir::Instruction *call) const;

    struct MultiCallInfo {
        struct SingleCallInfo {
            mir::Instruction *call;
            mir::Value *ret_location;
            mir::Function *called_func;
        };
        std::vector<SingleCallInfo> calls_info; // basic info extraction
        // for the caller save regs, where are they backup?
        std::map<mir::PhysicalRegister *, mir::Offset> ireg_loc, freg_loc;
        struct FreeSpace {
            mir::Offset off;
            mir::Offset size;
        };
        std::list<FreeSpace> free_space;

        bool first_call{true};
        mir::Offset stack_grow_size1{0}; // backup for caller saves
        mir::Offset stack_grow_size2{0}; // args passed on stack

        bool value_on_stack(mir::PhysicalRegister *reg) const {
            return not first_call and
                   (contains(ireg_loc, reg) or contains(freg_loc, reg));
        };
        void reset() {
            ireg_loc.clear(), freg_loc.clear(), free_space.clear();
            first_call = true;
            stack_grow_size1 = stack_grow_size2 = 0;
        }
    };
    void extract_basic_info(MultiCallInfo &info);
    void update_caller_saves(MultiCallInfo &info, unsigned idx, bool bef_call);
    void pass_args_stack(const ArgInfo &, MultiCallInfo &info);
    void pass_args_in_reg(const ArgInfo &, MultiCallInfo &info, bool for_float);
    void coordinate_func_ret(mir::Value *ret_location, MultiCallInfo &info);
    void recover_caller_save(MultiCallInfo &info);
    void caller_save_with_stack_grow(
        MultiCallInfo &info, const std::set<mir::PhysicalRegister *> &new_iregs,
        const std::set<mir::PhysicalRegister *> &new_fregs);

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

    // stack object in operands may be handled in different ways:
    // - Alloca: used for pointer, need to get its address
    // - Spilled: meaning it's spilled from a vreg, representing a value
    // - Callee Save: bad case
    //
    // @off_addition: typically the offset of stack_object is from frame info,
    // but stack may be changed at the time, so `off_addition` is used to get
    // correct location
    // @return: if tmp_addr_reg is overwriten
    // use insert inst only according to the function's semantic.
    bool distinguish_stack_usage(mir::PhysicalRegister *rd,
                                 mir::StackObject *stack_object,
                                 mir::IPReg *tmp_addr_reg,
                                 mir::Offset off_addition);

    mir::Instruction *safe_imm_inst(mir::MIR_INST op_imm, mir::MIR_INST op_rr,
                                    mir::IPReg *rd, mir::IPReg *rs, int imm,
                                    mir::Label *label = nullptr);

    mir::Instruction *insert_inst(mir::MIR_INST op,
                                  std::vector<mir::Value *> vec);
    mir::Instruction *gen_inst(mir::MIR_INST _op,
                               std::vector<mir::Value *> operands,
                               mir::Label *label);
    mir::Instruction *move_same_type(mir::Value *dest, mir::Value *src,
                                     mir::Label *label = nullptr);
    mir::Instruction *comment(std::string &&s, mir::Label *label = nullptr);

    // @return a set containing physical regs which hold valid value,
    // relying on `_upgrade_context.inst`
    std::set<mir::PhysicalRegister *>
    current_critical_regs(bool want_float, mir::PhysicalRegister::Saver s,
                          bool use_out_point = true) const;

    mir::MIR_INST load_store_op(mir::StackObject *, bool is_load);
};

}; // namespace codegen
