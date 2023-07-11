#include "mir_builder.hh"
#include "constant.hh"
#include "err.hh"
#include "instruction.hh"
#include "mir_config.hh"
#include "mir_immediate.hh"
#include "mir_memory.hh"
#include "mir_register.hh"
#include "mir_value.hh"
#include "type.hh"
#include "value.hh"
#include <new>

using namespace std;
using namespace mir;

map<ir::ICmpInst::ICmpOp, ir::ICmpInst::ICmpOp> ICMP_OP_REVERSED = {
    {ir::ICmpInst::EQ, ir::ICmpInst::NE}, {ir::ICmpInst::NE, ir::ICmpInst::EQ},
    {ir::ICmpInst::GT, ir::ICmpInst::LE}, {ir::ICmpInst::LE, ir::ICmpInst::GT},
    {ir::ICmpInst::GE, ir::ICmpInst::LT}, {ir::ICmpInst::LT, ir::ICmpInst::GE},
};

map<ir::FCmpInst::FCmpOp, ir::FCmpInst::FCmpOp> FCMP_OP_REVERSED = {
    {ir::FCmpInst::FEQ, ir::FCmpInst::FNE},
    {ir::FCmpInst::FNE, ir::FCmpInst::FEQ},
    {ir::FCmpInst::FGT, ir::FCmpInst::FLE},
    {ir::FCmpInst::FLE, ir::FCmpInst::FGT},
    {ir::FCmpInst::FGE, ir::FCmpInst::FLT},
    {ir::FCmpInst::FLT, ir::FCmpInst::FGE},
};

// FIXME
// - register reuse matters?
// - reserved instruction: how to deal, and how to mark?

MIRBuilder::MIRBuilder(unique_ptr<ir::Module> &&mod)
    : mir_moduler(new Module), ir_module(std::move(mod)),
      value_mgr(ValueManager::get()) {

    // external function
    memset_plt_func = create<Function>(false, "memset@plt", BasicType::VOID);

    for (auto &glob_var : ir_module->get_global_vars()) {
        value_map[&glob_var] = mir_moduler->add_global(&glob_var);
    }

    // map each ir-value to mir-value
    for (auto &ir_function : ir_module->get_functions()) {
        // function
        auto mir_funtion = mir_moduler->add_function(&ir_function);
        value_map[&ir_function] = mir_funtion;
        // argument
        auto &ir_args = ir_function.get_args();
        for (unsigned i = 0; i < ir_args.size(); ++i) {
            value_map[ir_args[i]] = mir_funtion->get_args(i);
        }
        // Blocks to label
        for (auto &BB : ir_function.get_bbs()) {
            auto label = mir_funtion->add_label(mir_funtion->get_name() + "." +
                                                BB.get_name());
            value_map[&BB] = label;
            for (auto &instruction : BB.get_insts()) {
                if (not should_save_map(&instruction))
                    continue;
                auto type = instruction.get_type();
                if (type->is<ir::FloatType>())
                    value_map[&instruction] = create<FVReg>();
                else if (type->is<ir::IntType>())
                    value_map[&instruction] = create<IVReg>();
                else if (type->is<ir::PointerType>()) {
                    if (is_a<const ir::AllocaInst>(&instruction)) {
                        auto alloc = as_a<const ir::AllocaInst>(&instruction);
                        value_map[&instruction] =
                            alloca_to_stack(alloc, mir_funtion);
                    } else
                        value_map[&instruction] = create<IVReg>();
                } else
                    throw unreachable_error{};
            }
        }
    }

    // generate mir-instruction, maintain prev-succ-info for labels
    for (auto &ir_function : ir_module->get_functions()) {
        cur_func = as_a<Function>(value_map.at(&ir_function));
        for (auto &BB : ir_function.get_bbs()) {
            cur_label = as_a<Label>(value_map.at(&BB));
            // maintain prev-succ-info for labels
            for (auto prev_bb : BB.get_pre_bbs())
                cur_label->add_prev(as_a<Label>(value_map.at(prev_bb)));
            for (auto succ_bb : BB.get_suc_bbs())
                cur_label->add_succ(as_a<Label>(value_map.at(succ_bb)));
            // translate ir-instruction to asm-instruction
            for (auto &inst : BB.get_insts()) {
                auto reg = inst.accept(this);
            }
        }
    }

    phi_elim_at_the_end();
}

// @reserved: ret-value must be in specific register
any MIRBuilder::visit(const ir::RetInst *instruction) {
    switch (cur_func->get_ret_type()) {
    case BasicType::VOID:
        cur_label->add_inst(Ret, {}, true);
        break;
    case BasicType::INT: {
        auto v = instruction->get_operand(0);
        auto imm_result = parse_imm(v);
        auto imm = imm_result.val;
        if (imm_result.is_undef) {
            cur_label->add_inst(Ret, {});
        } else if (imm_result.is_const) {
            if (Imm12bit::check_in_range(imm))
                cur_label->add_inst(Ret, {create<Imm12bit>(imm)}, true);
            else {
                auto reg = load_imm(imm);
                cur_label->add_inst(Ret, {reg}, true);
            }

        } else {
            cur_label->add_inst(Ret, {value_map.at(v)}, true);
        }
        break;
    }
    case BasicType::FLOAT:
        throw not_implemented_error{};
        break;
    }
    return {};
}

any MIRBuilder::visit(const ir::ICmpInst *instruction) { return {}; }

any MIRBuilder::visit(const ir::BrInst *instruction) {
    auto &operands = instruction->operands();
    if (operands.size() == 1) {
        auto label = value_map.at(operands[0]);
        cur_label->add_inst(Jump, {label});
        return {};
    }

    /* cond = icmp .. %op1, %op2
     * br i1 cond, TBB, FBB */
    auto [cond_src, TBB, FBB] =
        tuple(as_a<ir::Instruction>(operands[0]), operands[1], operands[2]);
    auto [TLabel, FLabel] = tuple(value_map.at(TBB), value_map.at(FBB));
    auto [op1, op2] = tuple(cond_src->operands()[0], cond_src->operands()[1]);
    auto [cond, reversed] = backtrace_i1(cond_src);

    if (is_a<ir::ICmpInst>(cond)) {
        auto icmp = as_a<ir::ICmpInst>(cond);
        // let related value in the register
        IVReg *reg1 = is_a<ir::ConstInt>(op1)
                          ? load_imm(as_a<ir::ConstInt>(op1)->val())
                          : as_a<IVReg>(value_map.at(op1));
        IVReg *reg2 = is_a<ir::ConstInt>(op2)
                          ? load_imm(as_a<ir::ConstInt>(op2)->val())
                          : as_a<IVReg>(value_map.at(op2));
        auto op = reversed ? ICMP_OP_REVERSED[icmp->get_icmp_op()]
                           : icmp->get_icmp_op();
        switch (op) {
        case ir::ICmpInst::EQ:
            cur_label->add_inst(BEQ, {reg1, reg2, TLabel});
            break;
        case ir::ICmpInst::NE:
            cur_label->add_inst(BNE, {reg1, reg2, TLabel});
            break;
        case ir::ICmpInst::GT:
            cur_label->add_inst(BLT, {reg2, reg1, TLabel});
            break;
        case ir::ICmpInst::GE:
            cur_label->add_inst(BGE, {reg1, reg2, TLabel});
            break;
        case ir::ICmpInst::LT:
            cur_label->add_inst(BLT, {reg1, reg2, TLabel});
            break;
        case ir::ICmpInst::LE:
            cur_label->add_inst(BGE, {reg2, reg1, TLabel});
            break;
        }
        cur_label->add_inst(Jump, {FLabel});
    } else if (is_a<ir::FCmpInst>(cond))
        throw not_implemented_error{};
    else
        throw unreachable_error{};

    return {};
}

pair<ir::Instruction *, bool> MIRBuilder::backtrace_i1(ir::Value *i1) {
    assert(i1->get_type()->is<ir::BoolType>());
    bool reversed = false;

    while (not is_a<ir::ICmpInst>(i1) and not is_a<ir::FCmpInst>(i1)) {
        assert(is_a<ir::IBinaryInst>(i1));
        auto int_binary_inst = as_a<ir::IBinaryInst>(i1);
        assert(int_binary_inst->get_ibin_op() == ir::IBinaryInst::XOR and
               int_binary_inst->operands()[1] ==
                   ir::Constants::get().bool_const(true));
        reversed = not reversed;
        i1 = int_binary_inst->operands()[0];
    }

    return {as_a<ir::Instruction>(i1), reversed};
}

any MIRBuilder::visit(const ir::ZextInst *instruction) {
    auto i1_src = instruction->operands()[0];
    auto imm_result = parse_imm(i1_src);
    if (imm_result.is_undef) {
        // FIXME if zext is on an undef value, there will be no asm instruction
        return {};
    }

    auto ret_reg = value_map.at(instruction);
    if (imm_result.is_const) {
        load_imm(as_a<ir::ConstInt>(i1_src)->val(), as_a<IVReg>(ret_reg));
        return {};
    }
    auto [i1, reversed] = backtrace_i1(i1_src);

    auto tmp_reg = create<IVReg>(); // FIXME unused maybe

    if (is_a<ir::ICmpInst>(i1)) {
        auto icmp = as_a<ir::ICmpInst>(i1);
        // let related value in the register, use RR mode to decrease complexity
        auto res =
            binary_helper(icmp->operands()[0], icmp->operands()[1], false);
        auto reg1 = res.op1;
        auto reg2 = res.op2;
        auto op = reversed ? ICMP_OP_REVERSED[icmp->get_icmp_op()]
                           : icmp->get_icmp_op();
        switch (op) {
        case ir::ICmpInst::GT:
            cur_label->add_inst(SLT, {ret_reg, reg2, reg1});
            break;
        case ir::ICmpInst::LT:
            cur_label->add_inst(SLT, {ret_reg, reg1, reg2});
            break;
        case ir::ICmpInst::GE:
            cur_label->add_inst(SLT, {tmp_reg, reg1, reg2});
            cur_label->add_inst(XORI, {ret_reg, tmp_reg, create<Imm12bit>(1)});
            break;
        case ir::ICmpInst::LE:
            cur_label->add_inst(SLT, {tmp_reg, reg2, reg1});
            cur_label->add_inst(XORI, {ret_reg, tmp_reg, create<Imm12bit>(1)});
            break;
        case ir::ICmpInst::EQ:
            cur_label->add_inst(XOR, {tmp_reg, reg1, reg2});
            cur_label->add_inst(SetEQZ, {ret_reg, tmp_reg});
            break;
        case ir::ICmpInst::NE:
            cur_label->add_inst(XOR, {tmp_reg, reg1, reg2});
            cur_label->add_inst(SetNEQZ, {ret_reg, tmp_reg});
            break;
        }
    } else if (is_a<ir::FCmpInst>(i1))
        throw not_implemented_error{};
    else
        throw unreachable_error{};

    return {};
}

any MIRBuilder::visit(const ir::IBinaryInst *instruction) {
    static const auto have_imm_version = {ir::IBinaryInst::ADD,
                                          ir::IBinaryInst::XOR};

    auto ibin_op = instruction->get_ibin_op();
    if (ibin_op == ir::IBinaryInst::XOR)
        return {};

    auto result_reg = value_map.at(instruction);
    auto operands = instruction->operands();
    auto res = binary_helper(operands[0], operands[1],
                             contains(have_imm_version, ibin_op));

    switch (ibin_op) {
    case ir::IBinaryInst::ADD:
        cur_label->add_inst(res.op2_is_imm ? ADDIW : ADDW,
                            {result_reg, res.op1, res.op2});
        break;
    case ir::IBinaryInst::SUB:
        cur_label->add_inst(SUBW, {result_reg, res.op1, res.op2});
        break;
    case ir::IBinaryInst::MUL:
        cur_label->add_inst(MULW, {result_reg, res.op1, res.op2});
        break;
    case ir::IBinaryInst::SDIV:
        cur_label->add_inst(DIVW, {result_reg, res.op1, res.op2});
        break;
    case ir::IBinaryInst::SREM:
        cur_label->add_inst(REMW, {result_reg, res.op1, res.op2});
        break;
    case ir::IBinaryInst::XOR:
        // xor use i1 and generate i1, leave for zext
        break;
    }

    return {};
}

// FIXME bug here
void MIRBuilder::phi_elim_at_the_end() {
    for (auto instruction : phi_list) {
        auto result_reg = value_map.at(instruction);
        auto &operands = instruction->operands();
        for (unsigned i = 0; i < operands.size(); i += 2) {
            auto irvalue = operands[i];
            auto prev_label = as_a<Label>(value_map.at(operands[i + 1]));

            auto imm_result = parse_imm(irvalue);
            if (imm_result.is_undef)
                continue;
            if (imm_result.is_const)
                prev_label->add_inst(
                    LoadImmediate,
                    {result_reg, create<Imm32bit>(imm_result.val)});
            else
                prev_label->add_inst(Move, {result_reg, value_map.at(irvalue)});
        }
    }
}

// append phi info at the end
any MIRBuilder::visit(const ir::PhiInst *instruction) {
    phi_list.push_back(instruction);
    return {};
}

// already parsed at first scan
any MIRBuilder::visit(const ir::AllocaInst *instruction) { return {}; }

// @reserved: address is partial
any MIRBuilder::visit(const ir::LoadInst *instruction) {
    if (not instruction->get_type()->is<ir::IntType>())
        throw not_implemented_error{};
    auto result_reg = value_map.at(instruction);
    auto [complete, address] = parse_address(instruction->operands()[0]);

    if (complete)
        cur_label->add_inst(LW, {result_reg, create<Imm12bit>(0), address});
    else
        cur_label->add_inst(LW, {result_reg, address}, true);

    return {};
}

any MIRBuilder::visit(const ir::StoreInst *instruction) {
    auto value = instruction->operands()[0];
    auto irptr = instruction->operands()[1];
    auto [complete, address] = parse_address(irptr);

    if (value->get_type()->is<ir::IntType>()) {
        auto imm_result = parse_imm(value);
        if (imm_result.is_undef) // do nothing for undef-store
            return {};
        auto value_reg = imm_result.is_const ? load_imm(imm_result.val)
                                             : value_map.at(value);
        if (complete)
            cur_label->add_inst(SW, {value_reg, create<Imm12bit>(0), address});
        else
            cur_label->add_inst(SW, {value_reg, address}, true);
    } else if (value->get_type()->is<ir::FloatType>()) {
        throw not_implemented_error{};
    } else if (value->get_type()->is<ir::ArrayType>()) {
        // store array is init-case
        auto arr_type = as_a<ir::ArrayType>(value->get_type());
        size_t total = arr_type->get_total_cnt();
        bool float_case = arr_type->get_base_type()->is<ir::FloatType>();
        if (float_case)
            throw not_implemented_error{};
        // ConstZero is not a ConstArray
        if (is_a<ir::ConstZero>(value)) {
            cur_label->add_inst(
                Call, {memset_plt_func, address, load_imm(0), load_imm(total)},
                true);
            return {};
        }
        InitPairs inits;
        auto const_arr = as_a<ir::ConstArray>(value);
        flatten_array(const_arr, inits);
        size_t assigns = inits.size();
        assert(assigns <= total);
        // FIXME we can use fine-grained method to store inits
        if (assigns != total) { // call memset@plt
            cur_label->add_inst(Call, {address, load_imm(0), load_imm(total)},
                                true);
        }
        for (auto [pos, v] : inits) {
            Value *value_reg;
            if (float_case)
                throw not_implemented_error{};
            else
                value_reg = load_imm(get<int>(v));
            int off = pos * BASIC_TYPE_SIZE;
            cur_label->add_inst(SW, {value_reg, create<Imm12bit>(off), address},
                                complete);
        }

    } else
        throw unreachable_error{};

    return {};
}

// create call instruction(reserved version) as:
// `call [r] function arg1 arg2...`
// r is the virtual register for return value
any MIRBuilder::visit(const ir::CallInst *instruction) {
    Value *ret_reg = nullptr;
    auto ret_type = instruction->get_type();
    if (not ret_type->is<ir::VoidType>()) {
        ret_reg = value_map.at(instruction);
    }

    auto operands = instruction->operands();
    auto mir_function = value_map.at(operands[0]);

    vector<Value *> ops;
    if (ret_reg)
        ops.push_back(ret_reg);
    ops.push_back(mir_function);
    for (auto i = 1; i < operands.size(); ++i) {
        auto value = operands[i];
        auto imm_result = parse_imm(value);
        Value *value_reg{nullptr};
        if (imm_result.is_undef) {
            if (value->get_type()->is<ir::FloatType>())
                value_reg = create<FVReg>();
            else
                value_reg = create<IVReg>();
        } else if (imm_result.is_const)
            value_reg = load_imm(imm_result.val);
        else
            value_reg = value_map.at(value);
        ops.push_back(value_reg);
    }

    cur_label->add_inst(Call, ops, true);

    return {};
}

any MIRBuilder::visit(const ir::GetElementPtrInst *instruction) {
    auto operands = instruction->operands();
    auto ptr_elem_type =
        as_a<ir::PointerType>(operands[0]->get_type())->get_elem_type();
    IVReg *offset_reg{nullptr}, *tmp_reg{nullptr};
    // first dim offset
    auto imm_result = parse_imm(operands[1]);
    assert(imm_result.is_undef == false);
    if (imm_result.is_const)
        offset_reg = load_imm(imm_result.val);
    else
        offset_reg = as_a<IVReg>(value_map.at(operands[1]));

    ir::Type *type = ptr_elem_type;
    for (unsigned i = 2; i < operands.size(); ++i) {
        auto arr_type = as_a<ir::ArrayType>(type);
        tmp_reg = load_imm(arr_type->get_elem_cnt());
        cur_label->add_inst(MUL, {offset_reg, offset_reg, tmp_reg});

        auto imm_result = parse_imm(operands[1]);
        assert(imm_result.is_undef == false);
        if (imm_result.is_const)
            tmp_reg = load_imm(imm_result.val);
        else
            tmp_reg = as_a<IVReg>(value_map.at(operands[i]));
        cur_label->add_inst(ADD, {offset_reg, offset_reg, tmp_reg});
        type = arr_type->get_elem_type();
    }

    size_t remaining_offset;
    if (type->is_basic_type())
        remaining_offset = BASIC_TYPE_SIZE;
    else
        remaining_offset =
            as_a<ir::ArrayType>(type)->get_total_cnt() * BASIC_TYPE_SIZE;
    tmp_reg = load_imm(remaining_offset);
    cur_label->add_inst(MUL, {offset_reg, offset_reg, tmp_reg});

    auto [complete, address] = parse_address(operands[0]);
    auto result_reg = value_map.at(instruction);
    cur_label->add_inst(ADD, {result_reg, address, offset_reg}, complete);

    return {};
}

any MIRBuilder::visit(const ir::FBinaryInst *instruction) {
    throw not_implemented_error{};
    return {};
}

any MIRBuilder::visit(const ir::FCmpInst *instruction) {
    throw not_implemented_error{};
    return {};
}

any MIRBuilder::visit(const ir::Fp2siInst *instruction) {
    throw not_implemented_error{};
    return {};
}

any MIRBuilder::visit(const ir::Si2fpInst *instruction) {
    throw not_implemented_error{};
    return {};
}
