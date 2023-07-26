#include "mir_builder.hh"
#include "constant.hh"
#include "err.hh"
#include "instruction.hh"
#include "mir_config.hh"
#include "mir_immediate.hh"
#include "mir_instruction.hh"
#include "mir_memory.hh"
#include "mir_register.hh"
#include "mir_value.hh"
#include "type.hh"
#include "value.hh"
#include <cassert>
#include <new>
#include <vector>

using namespace std;
using namespace mir;

const map<ir::ICmpInst::ICmpOp, ir::ICmpInst::ICmpOp> ICMP_OP_REVERSED = {
    {ir::ICmpInst::EQ, ir::ICmpInst::NE}, {ir::ICmpInst::NE, ir::ICmpInst::EQ},
    {ir::ICmpInst::GT, ir::ICmpInst::LE}, {ir::ICmpInst::LE, ir::ICmpInst::GT},
    {ir::ICmpInst::GE, ir::ICmpInst::LT}, {ir::ICmpInst::LT, ir::ICmpInst::GE},
};

const map<ir::FCmpInst::FCmpOp, ir::FCmpInst::FCmpOp> FCMP_OP_REVERSED = {
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
// - Are all pointer arithmetic operations 64 bit version now?

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
        if (mir_funtion->is_definition())
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
    case BasicType::INT:
    case BasicType::FLOAT: {
        auto v = instruction->get_operand(0);
        auto imm_result = parse_imm(v);
        auto imm = imm_result.val;
        if (imm_result.is_undef) {
            cur_label->add_inst(Ret, {});
        } else if (imm_result.is_const) {
            // const is float
            if (imm_result.is_float) {
                assert(cur_func->get_ret_type() == BasicType::FLOAT);
                auto reg = load_imm(imm);
                cur_label->add_inst(Ret, {reinterpret_i2f(reg)}, true);
                break;
            }
            // const is int
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
    default:
        throw unreachable_error{};
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
    auto [cond, reversed] = backtrace_i1(cond_src);
    auto [op1, op2] = tuple(cond->operands()[0], cond->operands()[1]);

    if (is_a<ir::ICmpInst>(cond)) {
        auto icmp = as_a<ir::ICmpInst>(cond);
        // let related value in the register
        // TODO: use x0 if possible
        IVReg *reg1 = is_a<ir::ConstInt>(op1)
                          ? load_imm(as_a<ir::ConstInt>(op1)->val())
                          : as_a<IVReg>(value_map.at(op1));
        IVReg *reg2 = is_a<ir::ConstInt>(op2)
                          ? load_imm(as_a<ir::ConstInt>(op2)->val())
                          : as_a<IVReg>(value_map.at(op2));
        auto op = reversed ? ICMP_OP_REVERSED.at(icmp->get_icmp_op())
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
    } else if (is_a<ir::FCmpInst>(cond)) {
        auto fcmp = as_a<ir::FCmpInst>(cond);
        FVReg *reg1{nullptr};
        if (is_a<ir::ConstFloat>(op1)) {
            auto fval1 = op1->as<ir::ConstFloat>()->val();
            reg1 = reinterpret_i2f(load_imm(reinterpret_cast<int &>(fval1)));
        } else {
            reg1 = as_a<FVReg>(value_map.at(op1));
        }
        FVReg *reg2{nullptr};
        if (is_a<ir::ConstFloat>(op2)) {
            auto fval2 = op2->as<ir::ConstFloat>()->val();
            reg2 = reinterpret_i2f(load_imm(reinterpret_cast<int &>(fval2)));
        } else {
            reg2 = as_a<FVReg>(value_map.at(op2));
        }
        auto op = reversed ? FCMP_OP_REVERSED.at(fcmp->get_fcmp_op())
                           : fcmp->get_fcmp_op();
        auto cmp_res = create<IVReg>();
        switch (op) {
        case ir::FCmpInst::FEQ:
            cur_label->add_inst(FEQS, {cmp_res, reg1, reg2});
            break;
        case ir::FCmpInst::FNE:
            cur_label->add_inst(FEQS, {cmp_res, reg1, reg2});
            break;
        case ir::FCmpInst::FGT:
            cur_label->add_inst(FLTS, {cmp_res, reg2, reg1});
            break;
        case ir::FCmpInst::FGE:
            cur_label->add_inst(FLES, {cmp_res, reg2, reg1});
            break;
        case ir::FCmpInst::FLT:
            cur_label->add_inst(FLTS, {cmp_res, reg1, reg2});
            break;
        case ir::FCmpInst::FLE:
            cur_label->add_inst(FLES, {cmp_res, reg1, reg2});
            break;
        default:
            throw unreachable_error{};
        }
        // TODO: use x0
        auto zero = load_imm(0);
        if (op == ir::FCmpInst::FNE) {
            cur_label->add_inst(BEQ, {zero, cmp_res, TLabel});
        } else {
            cur_label->add_inst(BNE, {zero, cmp_res, TLabel});
        }
        cur_label->add_inst(Jump, {FLabel});
    } else {
        throw unreachable_error{};
    }

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

    if (is_a<ir::ICmpInst>(i1)) {
        auto icmp = as_a<ir::ICmpInst>(i1);
        // let related value in the register, use RR mode to decrease complexity
        auto res =
            binary_helper(icmp->operands()[0], icmp->operands()[1], false);
        auto reg1 = res.op1;
        auto reg2 = res.op2;
        auto op = reversed ? ICMP_OP_REVERSED.at(icmp->get_icmp_op())
                           : icmp->get_icmp_op();
        switch (op) {
        case ir::ICmpInst::GT:
            cur_label->add_inst(SLT, {ret_reg, reg2, reg1});
            break;
        case ir::ICmpInst::LT:
            cur_label->add_inst(SLT, {ret_reg, reg1, reg2});
            break;
        case ir::ICmpInst::GE: {
            auto tmp_reg = create<IVReg>();
            cur_label->add_inst(SLT, {tmp_reg, reg1, reg2});
            cur_label->add_inst(XORI, {ret_reg, tmp_reg, create<Imm12bit>(1)});
            break;
        }
        case ir::ICmpInst::LE: {
            auto tmp_reg = create<IVReg>();
            cur_label->add_inst(SLT, {tmp_reg, reg2, reg1});
            cur_label->add_inst(XORI, {ret_reg, tmp_reg, create<Imm12bit>(1)});
            break;
        }
        case ir::ICmpInst::EQ: {
            auto tmp_reg = create<IVReg>();
            cur_label->add_inst(XOR, {tmp_reg, reg1, reg2});
            cur_label->add_inst(SetEQZ, {ret_reg, tmp_reg});
            break;
        }
        case ir::ICmpInst::NE: {
            auto tmp_reg = create<IVReg>();
            cur_label->add_inst(XOR, {tmp_reg, reg1, reg2});
            cur_label->add_inst(SetNEQZ, {ret_reg, tmp_reg});
            break;
        }
        default:
            throw unreachable_error{};
        }
    } else if (is_a<ir::FCmpInst>(i1)) {
        auto fcmp = as_a<ir::FCmpInst>(i1);
        // set register-imm mode to false, so that both operands are loaded into
        // interger register
        auto res =
            binary_helper(fcmp->operands()[0], fcmp->operands()[1], false);
        auto freg1 = reinterpret_i2f(as_a<IVReg>(res.op1));
        auto freg2 = reinterpret_i2f(as_a<IVReg>(res.op2));
        auto op = reversed ? FCMP_OP_REVERSED.at(fcmp->get_fcmp_op())
                           : fcmp->get_fcmp_op();
        switch (op) {
        case ir::FCmpInst::FGT:
            cur_label->add_inst(FLTS, {ret_reg, freg2, freg1});
            break;
        case ir::FCmpInst::FLT:
            cur_label->add_inst(FLTS, {ret_reg, freg1, freg2});
            break;
        case ir::FCmpInst::FGE:
            cur_label->add_inst(FLES, {ret_reg, freg2, freg1});
            break;
        case ir::FCmpInst::FLE:
            cur_label->add_inst(FLES, {ret_reg, freg1, freg2});
            break;
        case ir::FCmpInst::FEQ:
            cur_label->add_inst(FEQS, {ret_reg, freg1, freg2});
            break;
        case ir::FCmpInst::FNE: {
            auto tmp_reg = create<IVReg>();
            cur_label->add_inst(FEQS, {tmp_reg, freg1, freg2});
            // not
            cur_label->add_inst(XORI, {ret_reg, tmp_reg, create<Imm12bit>(1)});
            break;
        }
        default:
            throw unreachable_error{};
        }
    } else {
        throw unreachable_error{};
    }

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
        auto is_float = is_a<FVReg>(result_reg);
        for (unsigned i = 0; i < operands.size(); i += 2) {
            auto irvalue = operands[i];
            auto imm_result = parse_imm(irvalue);
            if (imm_result.is_undef)
                continue;
            auto prev_label = as_a<Label>(value_map.at(operands[i + 1]));
            auto first_branch = prev_label->get_first_branch();
            auto insert_inst = [&](MIR_INST op, vector<Value *> operands) {
                prev_label->insert_before(first_branch, op, operands);
            };
            if (imm_result.is_const) {
                assert(imm_result.is_float == is_float);
                if (is_float) {
                    auto ireg = create<IVReg>();
                    insert_inst(LoadImmediate,
                                {ireg, create<Imm32bit>(imm_result.val)});
                    insert_inst(FMVWX, {result_reg, ireg});
                } else
                    insert_inst(LoadImmediate,
                                {result_reg, create<Imm32bit>(imm_result.val)});
            } else
                insert_inst(is_float ? FMove : Move,
                            {result_reg, value_map.at(irvalue)});
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
    auto result_reg = value_map.at(instruction);
    auto [complete, address] = parse_address(instruction->operands()[0]);

    MIR_INST inst_type;
    if (instruction->get_type()->is<ir::IntType>()) {
        inst_type = LW;
    } else if (instruction->get_type()->is<ir::FloatType>()) {
        inst_type = FLW;
    } else {
        throw unreachable_error{};
    }

    if (complete) {
        cur_label->add_inst(inst_type,
                            {result_reg, create<Imm12bit>(0), address});
    } else {
        cur_label->add_inst(inst_type,
                            {result_reg, create<Imm12bit>(0), address}, true);
    }

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
        assert(not(imm_result.is_const and imm_result.is_float));
        cur_label->add_inst(SW, {value_reg, create<Imm12bit>(0), address});
    } else if (value->get_type()->is<ir::FloatType>()) {
        auto imm_result = parse_imm(value);
        assert(not imm_result.is_undef);
        Value *value_reg{nullptr};
        MIR_INST inst_type;
        if (imm_result.is_const) {
            assert(imm_result.is_float);
            inst_type = SW;
            value_reg = load_imm(imm_result.val);
        } else {
            inst_type = FSW;
            value_reg = value_map.at(value);
        }
        if (complete) {
            cur_label->add_inst(inst_type,
                                {value_reg, create<Imm12bit>(0), address});
        } else {
            cur_label->add_inst(inst_type, {value_reg, address}, true);
        }
    } else if (value->get_type()->is<ir::ArrayType>()) {
        // store array is init-case
        auto arr_type = as_a<ir::ArrayType>(value->get_type());
        size_t total = arr_type->get_total_cnt() * BASIC_TYPE_SIZE;
        bool float_case = arr_type->get_base_type()->is<ir::FloatType>();
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
            cur_label->add_inst(
                Call, {memset_plt_func, address, load_imm(0), load_imm(total)},
                true);
        }
        // FIXME: memcpy from rodata section instead of using a loooot of
        // immediates
        for (auto [pos, v] : inits) {
            int ival{0};
            if (float_case) {
                float fval = get<float>(v);
                ival = reinterpret_cast<int &>(fval);
            } else {
                ival = get<int>(v);
            }
            auto value_reg = load_imm(ival);
            int off = pos * BASIC_TYPE_SIZE;
            if (Imm12bit::check_in_range(off)) {
                cur_label->add_inst(
                    SW, {value_reg, create<Imm12bit>(off), address}, complete);
            } else {
                auto addr_reg = create<IVReg>();
                cur_label->add_inst(ADD, {addr_reg, load_imm(off), address},
                                    complete);
                cur_label->add_inst(SW,
                                    {value_reg, create<Imm12bit>(0), addr_reg});
            }
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
        } else if (imm_result.is_const) {
            if (imm_result.is_float) {
                value_reg = create<FImm32bit>(imm_result.val);
            } else {
                value_reg = create<Imm32bit>(imm_result.val);
            }
        } else
            value_reg = value_map.at(value);
        ops.push_back(value_reg);
    }

    cur_label->add_inst(Call, ops, true);

    return {};
}

any MIRBuilder::visit(const ir::GetElementPtrInst *instruction) {
    auto &operands = instruction->operands();

    auto arr_ptr = operands[0];
    IVReg *res_reg = as_a<IVReg>(value_map.at(instruction));

    // parse array index and element size
    vector<variant<int, Value *>> idxs;
    vector<int> sizes;
    auto elem_type =
        arr_ptr->get_type()->as<ir::PointerType>()->get_elem_type();
    for (size_t i = 2; i < operands.size(); i++) {
        auto elem_arr_type = elem_type->as<ir::ArrayType>();
        sizes.push_back(elem_arr_type->get_elem_cnt());
        elem_type = elem_arr_type->get_elem_type();
    }
    for (size_t i = 1; i < operands.size(); i++) {
        auto &op = operands[i];
        if (op->is<ir::ConstInt>()) {
            idxs.push_back(op->as<ir::ConstInt>()->val());
        } else {
            idxs.push_back(value_map.at(op));
        }
    }
    // take basic type size as a size
    if (elem_type->is_basic_type()) {
        sizes.push_back(BASIC_TYPE_SIZE);
    } else {
        sizes.push_back(elem_type->as<ir::ArrayType>()->get_total_cnt() *
                        BASIC_TYPE_SIZE);
    }
    // take array address as an index
    auto [is_address_complete, address] = parse_address(arr_ptr);
    idxs.push_back(address);
    assert(sizes.size() + 1 == idxs.size());

    // generate multi-add sequence for gep
    variant<int, Value *> off = 0;
    // insert a phantom size so that sizes.len == idxs.len
    sizes.insert(sizes.begin(), 1);
    auto is_leading_const = [&]() { return holds_alternative<int>(off); };
    for (size_t i = 0; i < idxs.size(); i++) {
        auto &idx = idxs[i];
        auto &size = sizes[i];

        // multiply
        if (is_leading_const()) {
            off = get<int>(off) * size;
        } else {
            auto off_reg = get<Value *>(off);
            cur_label->add_inst(MUL, {off_reg, off_reg, load_imm(size)});
        }

        // add
        if (is_leading_const() and holds_alternative<int>(idx)) {
            off = get<int>(idx) + get<int>(off);
        } else {
            if (is_leading_const()) {
                // FIXME: if off == 0, this li is redundant
                // encountered the first variable index, allocate reg for off
                off = load_imm(get<int>(off), res_reg);
            }
            auto off_reg = get<Value *>(off);
            auto idx_reg = holds_alternative<int>(idx) ? load_imm(get<int>(idx))
                                                       : get<Value *>(idx);
            // the last idx should be array's base address
            bool complete = (i == idxs.size() - 1) ? is_address_complete : true;
            cur_label->add_inst(ADD, {off_reg, off_reg, idx_reg}, complete);
        }
    }

    return {};
}

any MIRBuilder::visit(const ir::FBinaryInst *instruction) {
    auto fbin_op = instruction->get_fbin_op();

    auto result_reg = value_map.at(instruction);
    auto operands = instruction->operands();
    // load immediate to integer reg if exists
    auto res = binary_helper(operands[0], operands[1], false);
    if (is_a<IVReg>(res.op1)) {
        res.op1 = reinterpret_i2f(as_a<IVReg>(res.op1));
    }
    if (is_a<IVReg>(res.op2)) {
        res.op2 = reinterpret_i2f(as_a<IVReg>(res.op2));
    }

    switch (fbin_op) {
    case ir::FBinaryInst::FADD:
        cur_label->add_inst(FADDS, {result_reg, res.op1, res.op2});
        break;
    case ir::FBinaryInst::FSUB:
        cur_label->add_inst(FSUBS, {result_reg, res.op1, res.op2});
        break;
    case ir::FBinaryInst::FMUL:
        cur_label->add_inst(FMULS, {result_reg, res.op1, res.op2});
        break;
    case ir::FBinaryInst::FDIV:
        cur_label->add_inst(FDIVS, {result_reg, res.op1, res.op2});
        break;
    default:
        throw unreachable_error{};
    }
    return {};
}

any MIRBuilder::visit(const ir::FCmpInst *instruction) { return {}; }

any MIRBuilder::visit(const ir::Fp2siInst *instruction) {
    auto fop = instruction->operands()[0];
    auto imm = parse_imm(fop);
    assert(not imm.is_undef);
    auto res_reg = as_a<IVReg>(value_map.at(instruction));
    if (imm.is_const) {
        assert(imm.is_float);
        load_imm(reinterpret_cast<float &>(imm.val), res_reg);
    } else {
        auto freg = as_a<FVReg>(value_map.at(fop));
        cast_f2i(freg, res_reg);
    }
    return {};
}

any MIRBuilder::visit(const ir::Si2fpInst *instruction) {
    auto iop = instruction->operands()[0];
    auto imm = parse_imm(iop);
    assert(not imm.is_undef);
    auto res_reg = as_a<FVReg>(value_map.at(instruction));
    if (imm.is_const) {
        assert(not imm.is_float);
        float fval = imm.val;
        auto tmp_reg = load_imm(reinterpret_cast<int &>(fval));
        reinterpret_i2f(tmp_reg, res_reg);
    } else {
        auto ireg = as_a<IVReg>(value_map.at(iop));
        cast_i2f(ireg, res_reg);
    }
    return {};
}
