#pragma once

#include "constant.hh"
#include "err.hh"
#include "function.hh"
#include "global_variable.hh"
#include "instruction.hh"
#include "mir_config.hh"
#include "mir_function.hh"
#include "mir_immediate.hh"
#include "mir_instruction.hh"
#include "mir_label.hh"
#include "mir_memory.hh"
#include "mir_module.hh"
#include "mir_register.hh"
#include "mir_value.hh"
#include "module.hh"
#include "type.hh"
#include "value.hh"

#include <any>
#include <cassert>
#include <memory>
#include <stack>
#include <string>
#include <variant>
#include <vector>

using std::any;
using std::unique_ptr;

namespace mir {
class MIRBuilder : public ir::InstructionVisitor {
    friend class CodeGen;

  private:
    /* module */
    unique_ptr<Module> mir_moduler;
    unique_ptr<ir::Module> ir_module;

    /* runtime variable */
    Function *cur_func;
    Label *cur_label;

    /* core data structure */
    // map ir-value to mir-value
    std::unordered_map<const ir::Value *, Value *> value_map;
    std::vector<const ir::PhiInst *> phi_list;
    Function *memset_plt_func{nullptr};

    /* foreign ref */
    ValueManager &value_mgr;
    PhysicalRegisterManager &preg_mgr;

  public:
    explicit MIRBuilder(unique_ptr<ir::Module> &&mod);

    Module *release() {
        // free ir-module as well
        ir_module.reset();
        return mir_moduler.release();
    }

  private:
    template <class T, typename... Args> T *create(Args... args) {
        return value_mgr.create<T>(args...);
    }

    void phi_elim_at_the_end();

    // load Immediate into virtual register
    Register *load_imm(int imm, IVReg *target_reg = nullptr) {
        if (imm) {
            if (target_reg == nullptr)
                target_reg = create<IVReg>();
            cur_label->add_inst(LoadImmediate,
                                {target_reg, create<Imm32bit>(imm)});
        } else {
            if (target_reg == nullptr)
                return preg_mgr.zero();
            cur_label->add_inst(Move, {target_reg, preg_mgr.zero()});
        }
        return target_reg;
    }

    IVReg *load_global(ir::GlobalVariable *global_var,
                       IVReg *target_reg = nullptr) {
        if (target_reg == nullptr)
            target_reg = create<IVReg>();
        cur_label->add_inst(LoadAddress,
                            {target_reg, value_map.at(global_var)});
        return target_reg;
    }

    FVReg *cast_i2f(Register *ireg, FVReg *freg = nullptr) {
        ireg->assert_int();
        if (freg == nullptr) {
            freg = create<FVReg>();
        }
        cur_label->add_inst(FCVTSW, {freg, ireg});
        return freg;
    }

    FVReg *reinterpret_i2f(Register *ireg, FVReg *freg = nullptr) {
        ireg->assert_int();
        if (freg == nullptr) {
            freg = create<FVReg>();
        }
        cur_label->add_inst(FMVWX, {freg, ireg});
        return freg;
    }

    IVReg *cast_f2i(FVReg *freg, IVReg *ireg = nullptr) {
        if (ireg == nullptr) {
            ireg = create<IVReg>();
        }
        cur_label->add_inst(FCVTWS, {ireg, freg});
        return ireg;
    }

    IVReg *reinterpret_f2i(FVReg *freg, IVReg *ireg = nullptr) {
        if (ireg == nullptr) {
            ireg = create<IVReg>();
        }
        cur_label->add_inst(FMVXW, {ireg, freg});
        return ireg;
    }

    struct binary_helper_result {
        Value *op1{nullptr};
        Value *op2{nullptr};
        bool op2_is_imm{true}, is_reversed{false};
    };
    /* @brief: make 2 ir value 2 mir value, taking immediate into account
     * @RIMODE: general instruction has Reg-Reg mode and corresponding
     * Reg-Immediate mode. On false case, promise that 2 values are in register.
     * */
    binary_helper_result binary_helper(ir::Value *op1, ir::Value *op2,
                                       bool RIMODE = true) {
        binary_helper_result res;
        // auto [op1_const, op1_imm] = parse_imm(op1);
        // auto [op2_const, op2_imm] = parse_imm(op2);
        auto op1_imm_result = parse_imm(op1);
        auto op2_imm_result = parse_imm(op2);
        bool op1_can_be_imm = Imm12bit::check_in_range(op1_imm_result.val);
        bool op2_can_be_imm = Imm12bit::check_in_range(op2_imm_result.val);

        res.op2_is_imm =
            RIMODE and ((op1_imm_result.is_const and op1_can_be_imm) or
                        (op2_imm_result.is_const and op2_can_be_imm));
        res.is_reversed =
            res.op2_is_imm and not(op2_imm_result.is_const and op2_can_be_imm);

        if (not op1_imm_result.is_const)
            res.op1 = value_map.at(op1);
        else if (res.is_reversed)
            res.op1 = create<Imm12bit>(op1_imm_result.val);
        else
            res.op1 = load_imm(op1_imm_result.val);

        if (not op2_imm_result.is_const)
            res.op2 = value_map.at(op2);
        else if (res.op2_is_imm and not res.is_reversed)
            res.op2 = create<Imm12bit>(op2_imm_result.val);
        else
            res.op2 = load_imm(op2_imm_result.val);

        if (res.is_reversed)
            std::swap(res.op1, res.op2);

        return res;
    }

    StackObject *alloca_to_stack(const ir::AllocaInst *instruction,
                                 Function *func) {
        size_t size = 0;
        size_t alignment = 0;
        auto alloca_type =
            as_a<ir::PointerType>(instruction->get_type())->get_elem_type();
        ir::Type *ir_basic_type;
        if (alloca_type->is_basic_type()) {
            size = BASIC_TYPE_SIZE;
            alignment = BASIC_TYPE_ALIGN;
            ir_basic_type = alloca_type;
        } else if (alloca_type->is<ir::ArrayType>()) {
            auto arr_type = as_a<ir::ArrayType>(alloca_type);
            size = BASIC_TYPE_SIZE * arr_type->get_total_cnt();
            alignment = BASIC_TYPE_ALIGN;
            ir_basic_type = arr_type->get_base_type();
        } else
            throw unreachable_error{};
        assert(ir_basic_type->is_basic_type());
        BasicType type = ir_basic_type->is<ir::IntType>() ? BasicType::INT
                                                          : BasicType::FLOAT;
        return func->add_local_var(type, size, alignment,
                                   StackObject::Reason::Alloca);
    }

    // NOTE the bool return which means complete is aborted now
    std::pair<bool, Value *> parse_address(ir::Value *irptr) {
        if (is_a<ir::GlobalVariable>(irptr)) {
            return {true, load_global(as_a<ir::GlobalVariable>(irptr))};
        } else if (is_a<ir::GetElementPtrInst>(irptr)) {
            return {true, value_map.at(irptr)};
        } else if (is_a<ir::Argument>(irptr)) {
            return {true, value_map.at(irptr)};
        } else if (is_a<ir::AllocaInst>(irptr)) {
            return {false, value_map.at(irptr)};
        } else if (is_a<ir::Int2PtrInst>(irptr))
            return {true, value_map.at(irptr)};
        else
            throw unreachable_error{};
    }

    /*
    if is_undef
        undef
    else if is_const
        if is_float
            float
        else
            int
    else
        other ir value
    */
    struct parse_imm_result {
        bool is_undef{false};
        bool is_const{true};
        bool is_float{false};
        int val{-1};
    };
    // @brief: return immediate value with flag
    // FIXME integer for now
    static parse_imm_result parse_imm(ir::Value *v) {
        parse_imm_result result;
        if (is_a<ir::Constant>(v)) {
            if (is_a<ir::ConstInt>(v)) {
                result.val = as_a<ir::ConstInt>(v)->val();
            } else if (is_a<ir::ConstBool>(v)) {
                result.val = as_a<ir::ConstBool>(v)->val();
            } else if (is_a<ir::ConstZero>(v)) {
                result.val = 0;
            } else if (is_a<ir::Undef>(v)) {
                result.is_undef = true;
            } else if (is_a<ir::ConstFloat>(v)) {
                auto fval = as_a<ir::ConstFloat>(v)->val();
                result.val = reinterpret_cast<int &>(fval);
                result.is_float = true;
            } else {
                throw unreachable_error{};
            }
        } else {
            result.is_const = false;
        }
        return result;
    }

    // save specific instruction-register map
    static bool should_save_map(const ir::Instruction *instruction) {
        auto inst_type = instruction->get_type();
        if (not inst_type->is<ir::VoidType>() and
            not inst_type->is<ir::BoolType>())
            return true;
        return false;
    }

    static bool is_int_reg(const Value *v) {
        if (not is_a<const Register>(v))
            return false;
        auto reg = as_a<const Register>(v);
        return reg->is_int_register();
    }

    /* @brief: backtrace the i1 origin, return a icmp or fcmp instruction.
     * i1 origin: cmp/fcmp, xor
     * and on xor case, xor is always used as: %op = xor %op true
     * */
    static std::pair<ir::Instruction *, bool> backtrace_i1(ir::Value *i1);

    // use the convenience of the implicit type convert
    static inline Value *to_base_type(Value *v) { return v; }

    virtual any visit(const ir::RetInst *instruction) override final;
    virtual any visit(const ir::BrInst *instruction) override final;
    virtual any visit(const ir::IBinaryInst *instruction) override final;
    virtual any visit(const ir::FBinaryInst *instruction) override final;
    virtual any visit(const ir::AllocaInst *instruction) override final;
    virtual any visit(const ir::LoadInst *instruction) override final;
    virtual any visit(const ir::StoreInst *instruction) override final;
    virtual any visit(const ir::ICmpInst *instruction) override final;
    virtual any visit(const ir::FCmpInst *instruction) override final;
    virtual any visit(const ir::PhiInst *instruction) override final;
    virtual any visit(const ir::CallInst *instruction) override final;
    virtual any visit(const ir::Fp2siInst *instruction) override final;
    virtual any visit(const ir::Si2fpInst *instruction) override final;
    virtual any visit(const ir::GetElementPtrInst *instruction) override final;
    virtual any visit(const ir::ZextInst *instruction) override final;
    virtual any visit(const ir::Ptr2IntInst *instruction) override final;
    virtual any visit(const ir::Int2PtrInst *instruction) override final;

    // specialized inst builder
    void build_sdiv_by_const(Value *res, Value *n, int d);
    void build_mul_by_const(Value *res, Value *n, int d);
    bool build_sdiv_by_const(const ir::IBinaryInst *inst);
    bool build_srem_by_const(const ir::IBinaryInst *inst);
    bool build_mul_by_const(const ir::IBinaryInst *inst);
};
} // namespace mir
