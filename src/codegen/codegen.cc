#include "codegen.hh"
#include "context.hh"
#include "err.hh"
#include "mir_builder.hh"
#include "mir_config.hh"
#include "mir_function.hh"
#include "mir_immediate.hh"
#include "mir_instruction.hh"
#include "mir_label.hh"
#include "mir_memory.hh"
#include "mir_register.hh"
#include "mir_value.hh"
#include "regalloc.hh"
#include "utils.hh"

#include <algorithm>
#include <cassert>
#include <deque>
#include <functional>
#include <stack>
#include <utility>
#include <variant>
#include <vector>

// helper type for the visitor #4
template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
// explicit deduction guide (not needed as of C++20)
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

using namespace std;
using namespace codegen;
using namespace mir;
using namespace context;

using Saver = PhysicalRegister::Saver;
using LabelType = Label::LabelType;
using Reason = StackObject::Reason;
using RegIDType = Register::RegIDType;

// TODO
// - distinguish register used bits for int registers: when to use lw/ld
// - in mir_builder, we can leave immediate(int only for now) in the operands,
// codegen will take care of it

auto &preg_mgr = PhysicalRegisterManager::get();
auto &value_mgr = ValueManager::get();
auto sp = preg_mgr.sp();
auto t0 = preg_mgr.temp(0);
auto t1 = preg_mgr.temp(1);
auto ft0 = preg_mgr.ftemp(0);
const std::map<PhysicalRegister *, int> tmp_arg_off = {
    {t0, -8},
    {t1, -16},
    {ft0, -20},
};

Immediate *create_imm(int imm, int bits = 12) {
    switch (bits) {
    case 12:
        return value_mgr.create<Imm12bit>(imm);
        break;
    case 32:
        return value_mgr.create<Imm32bit>(imm);
        break;
    default:
        throw not_implemented_error{};
    }
};

// the stack may be large, its size is represented using Offset inside program,
// this is to make sure that no overflow occurs during type cast from Offset to
// int
inline int Offset2int(Offset offset) {
    int off = offset;
    assert(off >= 0);
    assert(off == offset);
    return off;
}

// helpers to maintain prev/succ info
void link(Label *src_label, Label *dest_label) {
    src_label->add_succ(dest_label);
    dest_label->add_prev(src_label);
}
void unlink(Label *src_label, Label *dest_label) {
    src_label->rm_succ(dest_label);
    dest_label->rm_prev(src_label);
}

Instruction *CodeGen::insert_inst(mir::MIR_INST op,
                                  std::vector<mir::Value *> vec) {
    auto label = _upgrade_context.label;
    auto inst = _upgrade_context.inst;
    return &label->insert_before(inst, op, vec);
}

Instruction *CodeGen::gen_inst(MIR_INST _op, vector<Value *> operands,
                               Label *label) {
    if (label)
        return &label->add_inst(_op, operands);
    else
        return insert_inst(_op, operands);
};

bool CodeGen::safe_load_store(MIR_INST op, PhysicalRegister *rd_or_rs,
                              int offset, IPReg *tmp_reg, mir::Label *label) {
    if (Imm12bit::check_in_range(offset)) {
        gen_inst(op, {rd_or_rs, create_imm(offset), sp}, label);
        return false;
    } else {
        assert(tmp_reg != nullptr);
        gen_inst(LoadImmediate, {tmp_reg, create_imm(offset, 32)}, label);
        gen_inst(ADD, {tmp_reg, tmp_reg, sp}, label);
        auto target_inst =
            gen_inst(op, {rd_or_rs, create_imm(0), tmp_reg}, label);
        if (not target_inst->will_write_register() and tmp_reg == rd_or_rs)
            throw unreachable_error{
                "Sorry bug predicted here, same reg for temp and source, the "
                "register has been overwriten, source value is invalid now"};
        return true;
    }
}

bool CodeGen::stack_change(int delta, mir::IPReg *tmp_reg, Label *label) {
    if (delta == 0)
        return false;

    if (Imm12bit::check_in_range(delta)) {
        gen_inst(ADDI, {sp, sp, create_imm(delta)}, label);
        return false;
    } else {
        assert(tmp_reg != nullptr);
        gen_inst(LoadImmediate, {tmp_reg, create_imm(delta, 32)}, label);
        gen_inst(ADD, {sp, sp, tmp_reg}, label);
        return true;
    }
};

Instruction *CodeGen::move_same_type(Value *dest, Value *src, Label *label) {
    if (src == dest)
        return nullptr;
    if (is_a<FPReg>(dest) and is_a<FPReg>(src)) {
        return gen_inst(FMove, {dest, src}, label);
    } else if (is_a<IPReg>(dest) and is_a<IPReg>(src)) {
        return gen_inst(Move, {dest, src}, label);
    } else
        throw unreachable_error{"register is not same type"};
}

Instruction *CodeGen::safe_imm_inst(MIR_INST op_imm, MIR_INST op_rr, IPReg *rd,
                                    IPReg *rs, int imm, Label *label) {
    if (Imm12bit::check_in_range(imm))
        return gen_inst(op_imm, {rd, rs, create_imm(imm)}, label);
    else {
        gen_inst(LoadImmediate, {rd, create_imm(imm, 32)}, label);
        return gen_inst(op_rr, {rd, rs, rd}, label);
    }
}
bool CodeGen::distinguish_stack_usage(PhysicalRegister *rd,
                                      StackObject *stack_object,
                                      IPReg *tmp_addr_reg,
                                      Offset off_addition) {
    auto &func_frame = _upgrade_context.func->get_frame();
    int cur_off = Offset2int(func_frame.offset.at(stack_object) + off_addition);

    switch (stack_object->get_reason()) {
    case StackObject::Reason::Alloca: { // need address
        assert(is_a<IPReg>(rd));
        auto int_rd = as_a<IPReg>(rd);
        safe_imm_inst(ADDI, ADD, int_rd, sp, cur_off);
        return false;
    }
    case StackObject::Reason::ArgsOnStack:
    case StackObject::Reason::Spilled: // need value
        return safe_load_store(stack_object->load_op(), rd, cur_off,
                               tmp_addr_reg);
    case StackObject::Reason::CalleeSave:
        throw unreachable_error{};
    }
}

Instruction *CodeGen::comment(string &&s, Label *label) {
    return gen_inst(COMMENT, {value_mgr.create<Comment>(s)}, label);
}

CodeGen::ArgInfo CodeGen::split_func_args_logue_ver() const {
    auto func = _upgrade_context.func;
    auto &arg_spilled_location = _upgrade_context.arg_spilled_location;
    auto &ireg_map = _allocator.get_reg_map(func, false);
    auto &freg_map = _allocator.get_reg_map(func, true);
    ArgInfo info;
    unsigned i_idx = 0, f_idx = 0;
    for (auto arg : func->get_args()) {
        bool is_float = is_a<FVReg>(arg);
        auto &reg_map = is_float ? freg_map : ireg_map;
        auto &info_arr =
            is_float ? info.float_args_in_reg : info.int_args_in_reg;
        auto &idx = is_float ? f_idx : i_idx;

        decltype(ArgInfo::_info::location) location;
        if (contains(arg_spilled_location, arg)) {
            location = arg_spilled_location.at(arg);
        } else {
            auto preg_id = reg_map.at(arg->get_id()).reg->get_id();
            if (is_float)
                location = preg_mgr.get_float_reg(preg_id);
            else
                location = preg_mgr.get_int_reg(preg_id);
        }

        ArgInfo::_info _info{true, is_float, location};

        if (idx < 8)
            info_arr.at(idx++) = _info;
        else
            info.args_in_stack.push_back(_info);
    }
    return info;
}

void CodeGen::coordinate_func_args() {
    auto func = _upgrade_context.func;
    auto entry = _upgrade_context.entry;
    PhysicalRegister *convetion_reg = nullptr;
    set<PhysicalRegister *> reg_in_use;
    map<PhysicalRegister *, bool> tmp_reg_need_recover{
        {t0, false}, {t1, false}, {ft0, false}};

    /* if a tmp reg is to be used and it holds valid value now, backup it */
    auto tmp_reg_backup_check = [&](PhysicalRegister *tmp_reg) {
        if (not contains(reg_in_use, tmp_reg))
            return;
        auto store_op = is_a<FPReg>(tmp_reg) ? FSW : SD;
        safe_load_store(store_op, tmp_reg, tmp_arg_off.at(tmp_reg), nullptr,
                        entry);
        reg_in_use.erase(tmp_reg);
        tmp_reg_need_recover.at(tmp_reg) = true;
    };
    /* for first 8 args, have convetion_reg in calling convetion
     * - if has a physical reg `r`, move into a{} into `r`
     * - if spilled during reg-allocation, store a{} into mem */
    auto ArgResolver = overloaded{
        // arg is allocated to stack during reg allocation
        [&](StackObject *stack_object) {
            assert(not contains(reg_in_use, convetion_reg));
            assert(stack_object->get_reason() != Reason::ArgsOnStack);
            auto off = func->get_frame().offset.at(stack_object);
            MIR_INST store_op = stack_object->store_op();
            tmp_reg_backup_check(t1);
            safe_load_store(store_op, convetion_reg, Offset2int(off), t1,
                            entry);
        },
        // arg is allocated a physical reg during reg allocation
        [&](PhysicalRegister *preg) {
            assert(not contains(reg_in_use, convetion_reg));
            gen_inst(COMMENT,
                     {preg, value_mgr.create<Comment>("="), convetion_reg},
                     entry);
            if (preg != convetion_reg)
                move_same_type(preg, convetion_reg, entry);
            reg_in_use.insert(preg);
        },
        [&](mir::Immediate *) {
            throw unreachable_error{"invalid for logue case"};
        },
    };
    auto arg_info = split_func_args_logue_ver();
    // a0~a7
    for (unsigned i = 0; i < 8; ++i) {
        convetion_reg = preg_mgr.arg(i);
        auto info = arg_info.int_args_in_reg[i];
        if (not info.valid)
            break;
        visit(ArgResolver, info.location);
    }
    // fa0~fa7
    for (unsigned i = 0; i < 8; ++i) {
        convetion_reg = preg_mgr.farg(i);
        auto info = arg_info.float_args_in_reg[i];
        if (not info.valid)
            break;
        visit(ArgResolver, info.location);
    }

    // reorder args passed by stack,
    // spilled vreg first, then vreg mapped to preg
    auto frame_size = func->get_frame().size;
    vector<pair<Offset, ArgInfo::_info>> reordered_args_in_stack;
    // the last preg mapped to would become convetion reg so that it wouldn't be
    // overwriten by previous use as a tmp reg
    PhysicalRegister *int_convetion_reg{t0}, *float_convetion_reg{ft0};
    for (size_t i = 0; i < arg_info.args_in_stack.size(); i++) {
        auto &&info = arg_info.args_in_stack[i];
        auto off = frame_size + i * TARGET_MACHINE_SIZE;
        if (holds_alternative<PhysicalRegister *>(info.location)) {
            reordered_args_in_stack.push_back({off, info});
            auto preg = get<PhysicalRegister *>(info.location);
            if (info.is_float) {
                float_convetion_reg = preg;
            } else {
                int_convetion_reg = preg;
            }
        } else if (holds_alternative<StackObject *>(info.location)) {
            assert(get<StackObject *>(info.location)->get_reason() ==
                   Reason::ArgsOnStack);
        } else {
            throw unreachable_error{};
        }
    }
    // arguments passed on stack
    for (auto &&[off, stack_arg_info] : reordered_args_in_stack) {
        MIR_INST load_op;
        if (stack_arg_info.is_float) {
            convetion_reg = float_convetion_reg;
            load_op = FLW;
        } else {
            convetion_reg = int_convetion_reg;
            load_op = LD; // FIXME maybe ok?
        }
        auto tmp_reg = int_convetion_reg == t0 ? t1 : t0;
        // check if a backup is needed
        tmp_reg_backup_check(convetion_reg);
        tmp_reg_backup_check(tmp_reg);
        // load value and move to the allocated location
        safe_load_store(load_op, convetion_reg, Offset2int(off), tmp_reg,
                        entry);
        visit(ArgResolver, stack_arg_info.location);
    }

    // recover tmp regs if used
    for (auto [reg, need_recover] : tmp_reg_need_recover) {
        auto load_op = is_a<FPReg>(reg) ? FLW : LD;
        if (need_recover)
            safe_load_store(load_op, reg, tmp_arg_off.at(reg), nullptr, entry);
    }
}

// resolve prologue and epilogue
void CodeGen::resolve_logue() {
    auto func = _upgrade_context.func;
    auto entry = _upgrade_context.entry;
    auto exit = _upgrade_context.exit;
    auto &frame = func->get_frame();

    comment("prologue", entry);
    comment("epilogue", exit);

    // no frame relation need to maintain
    if (frame.size == 0) {
        exit->add_inst(Ret, {});
        return;
    }

    auto resolve_callee = [&](bool backup) {
        auto label = backup ? entry : exit;
        for (auto save : func->get_callee_saves()) {
            auto op = save->mem_op(not backup);
            auto reg =
                preg_mgr.get_reg(save->saved_reg_id(), save->is_float_reg());
            auto off = Offset2int(frame.offset.at(save));
            safe_load_store(op, reg, off, t0, label);
        }
    };

    /* prologue */
    comment("stack grow", entry);
    stack_change(-Offset2int(frame.size), t0, entry); // stack grow
    comment("callee backup", entry);
    resolve_callee(true); // save callee regs

    /* resolve args */
    comment("coordinate with convetion reg", entry);
    coordinate_func_args();

    // fp: we do not use it for now
    // entry->add_inst(ADDI, {preg_mgr.fp(), sp, create_imm(frame_size)});

    /* epilogue */
    comment("callee recover", exit);
    resolve_callee(false);
    comment("stack recover", exit);
    stack_change(Offset2int(frame.size), t0, exit);

    // return value is handled by prev labels
    exit->add_inst(Ret, {});
}

// TODO float
void CodeGen::upgrade() {
    _stage = Stage::stage2;

    for (auto func : _mir_module->get_functions()) {
        if (not func->is_definition())
            continue;
        _upgrade_context.new_func(func);
        upgrade_step1();
        func->allocate_location(); // allocate stack location
        upgrade_step2();
    }
}

std::ostream &codegen::operator<<(std::ostream &os, const CodeGen &c) {
    switch (c._stage) {
    case Stage::stage1: {
        os << "# stage1, uncomplete asm\n";
        break;
    }
    case Stage::stage2:
        os << "# stage2, complete asm\n";
        // main is a global symbol
        os << ".global main\n";
        break;
    }

    Context context{c._stage, Role::Full, c._allocator, true};
    c._mir_module->dump(os, context);

    return os;
}

set<PhysicalRegister *> CodeGen::current_critical_regs(bool want_float,
                                                       Saver saver) const {
    auto func = _upgrade_context.func;
    auto inst = _upgrade_context.inst;

    set<PhysicalRegister *> critical_regs;

    auto instid = _allocator.get_cfg_info(func).instid.at(inst);
    auto &out_set =
        _allocator.get_liveness(func, want_float).at(OUT_POINT(instid));
    auto &reg_map = _allocator.get_reg_map(func, want_float);
    for (auto vreg : out_set) {
        if (contains(reg_map, vreg)) {
            auto preg = reg_map.at(vreg).reg;
            if (saver == preg->get_saver())
                critical_regs.insert(preg);
        }
    }

    return critical_regs;
}

void CodeGen::upgrade_step1() {
    auto func = _upgrade_context.func;

    auto &int_reg_map = _allocator.get_reg_map(func, false);
    auto &int_spilled = _allocator.get_spilled(func, false);
    auto &float_reg_map = _allocator.get_reg_map(func, true);
    auto &float_spilled = _allocator.get_spilled(func, true);
    map<RegIDType, StackObject *> int_spilled_location, float_spilled_location;
    set<RegIDType> int_callee_saves, float_callee_saves;

    /* spilled virtual reg to stack */
    // first deal with arguments
    unsigned iarg_cnt = 0, farg_cnt = 0, stack_cnt = 0;
    for (auto arg : func->get_args()) {
        bool is_float = is_a<FVReg>(arg);
        auto &arg_cnt = is_float ? farg_cnt : iarg_cnt;
        auto &spilled_set = is_float ? float_spilled : int_spilled;
        if (arg_cnt++ >= 8)
            stack_cnt++; // is a reg passed on stack
        if (not contains(spilled_set, arg->get_id()))
            continue;
        if (arg_cnt <= 8) {
            // for first 8 arg passed on reg(but spilled in cur func), create a
            // local space, so leave it for later resolve
        } else {
            auto basic_type = is_float ? BasicType::FLOAT : BasicType::INT;
            auto &spilled_location =
                is_float ? float_spilled_location : int_spilled_location;
            spilled_location.insert(
                {arg->get_id(),
                 func->add_arg_on_caller_stack(basic_type, stack_cnt - 1)});
        }
    }

    for (auto spill_id : int_spilled) {
        if (contains(int_spilled_location, spill_id))
            continue;
        // FIXME use more specific size(only pointer use 8 bytes)
        int_spilled_location.insert(
            {spill_id,
             func->add_local_var(BasicType::INT, TARGET_MACHINE_SIZE,
                                 TARGET_MACHINE_SIZE, Reason::Spilled)});
    }
    for (auto spill_id : float_spilled) {
        if (contains(float_spilled_location, spill_id))
            continue;
        float_spilled_location.insert(
            {spill_id, func->add_local_var(BasicType::FLOAT, BASIC_TYPE_SIZE,
                                           BASIC_TYPE_SIZE, Reason::Spilled)});
    }

    // 1st pass
    for (auto label : func->get_labels()) {
        for (auto &inst : label->get_insts()) {
            bool inst_will_write_reg = inst.will_write_register();
            // special judgment for call's dead code
            if (inst.get_opcode() == Call and inst.will_write_register()) {
                bool is_float = is_a<FVReg>(inst.get_operand(0));
                auto ret_reg = as_a<Register>(inst.get_operand(0))->get_id();
                auto &reg_map = is_float ? float_reg_map : int_reg_map;
                auto &spilled_set = is_float ? float_spilled : int_spilled;
                if (not contains(spilled_set, ret_reg) and
                    not contains(reg_map, ret_reg))
                    inst.operands().erase(inst.operands().begin());
            }

            // special case for ra:
            // - if ra is writen as a general register, it will be add in to
            // callee-saves during the vreg replacement
            // - if ra is writen implicitly during call, the code below will
            // handle it
            if (inst.get_opcode() == Call)
                int_callee_saves.insert(preg_mgr.ra()->get_id());

            for (unsigned i = 0; i < inst.get_operand_num(); ++i) {
                // the folowing code will run only if op is a vreg
                auto op = inst.get_operand(i);
                if (not is_a<VirtualRegister>(op))
                    continue;
                bool is_float;
                if (is_a<IVReg>(op))
                    is_float = false;
                else if (is_a<FVReg>(op))
                    is_float = true;
                else
                    throw unreachable_error{"register should be virtual here"};
                auto vid = as_a<Register>(op)->get_id();
                auto &spilled_set = is_float ? float_spilled : int_spilled;
                auto &reg_map = is_float ? float_reg_map : int_reg_map;
                auto &spilled_location =
                    is_float ? float_spilled_location : int_spilled_location;
                auto &callee_saves =
                    is_float ? float_callee_saves : int_callee_saves;

                // tasks:
                // 1. eliminate virtual registers: spilled and mapped
                // 2. record callee saves
                if (contains(spilled_set, vid))
                    inst.set_operand(i, spilled_location.at(vid));
                else {
                    auto preg_id = reg_map.at(vid).reg->get_id();
                    auto preg = preg_mgr.get_reg(preg_id, is_float);
                    inst.set_operand(i, preg);
                    if (inst_will_write_reg and i == 0 and
                        preg->get_saver() == Saver::Callee)
                        callee_saves.insert(preg_id);
                }
            }
        }
    }

    // add physical regs used by function arguments to callee_saves
    for (auto arg : func->get_args()) {
        bool is_float = is_a<FVReg>(arg);
        auto &spilled_set = is_float ? float_spilled : int_spilled;
        auto &reg_map = is_float ? float_reg_map : int_reg_map;
        auto &callee_saves = is_float ? float_callee_saves : int_callee_saves;
        auto vreg_id = arg->get_id();
        if (not contains(spilled_set, vreg_id)) {
            auto preg_id = reg_map.at(vreg_id).reg->get_id();
            auto preg = preg_mgr.get_reg(preg_id, is_float);
            if (preg->get_saver() == Saver::Callee) {
                callee_saves.insert(preg_id);
            }
        }
    }

    // add callee saves to function
    for (auto preg_id : int_callee_saves)
        func->add_callee_save(BasicType::INT, preg_id);
    for (auto preg_id : float_callee_saves)
        func->add_callee_save(BasicType::FLOAT, preg_id);

    // update context: argument spill info
    for (auto arg : func->get_args()) {
        bool is_float = is_a<FVReg>(arg);
        auto &spilled_set = is_a<FVReg>(arg) ? float_spilled : int_spilled;
        auto &spilled_location =
            is_float ? float_spilled_location : int_spilled_location;
        auto arg_id = arg->get_id();
        if (not contains(spilled_set, arg_id))
            continue;
        _upgrade_context.arg_spilled_location.insert(
            {arg, spilled_location.at(arg_id)});
    }
}

void CodeGen::upgrade_step2() {
    auto func = _upgrade_context.func;

    // create new block
    auto entry = _upgrade_context.entry = func->create_entry();
    auto exit = _upgrade_context.exit = func->create_exit();
    resolve_logue();
    _upgrade_context.new_labels.insert(entry);
    _upgrade_context.new_labels.insert(exit);

    // 2nd pass
    // the traversing method matters
    for (unsigned _li = 0; _li < func->get_labels().size(); ++_li) {
        auto label = func->get_labels()[_li];
        _upgrade_context.label = label;

        if (contains(_upgrade_context.new_labels, label))
            continue;

        for (auto iter = label->get_insts().begin();
             iter != label->get_insts().end();) {
            auto inst = &*iter;
            ++iter;
            _upgrade_context.inst = inst;

            if (inst->get_opcode() == COMMENT)
                continue;

            switch (inst->get_opcode()) {
            case Ret:
                resolve_ret();
                break;
            case Call:
                resolve_call();
                break;
            case Move:
            case FMove:
                resolve_move();
                break;
            default:
                resolve_stack();
                break;
            }
        }
    }
}

// fill value into a0/fa0(new instruction)
void CodeGen::resolve_ret() {
    auto func = _upgrade_context.func;
    auto label = _upgrade_context.label;
    auto exit = _upgrade_context.exit;
    auto inst = _upgrade_context.inst;
    assert(&label->get_insts().back() == inst); // ret is the last inst
    assert(inst->get_operand_num() <= 1);       // operands validaty
    if (inst->get_operand_num() == 1) {         // fill return value
        // physical register
        auto is_float = func->get_ret_type() == BasicType::FLOAT;
        auto value_reg = preg_mgr.get_ret_val_reg(0, is_float);
        // return value's location
        auto ret_value_location = inst->get_operand(0);
        if (is_a<Immediate>(ret_value_location)) {
            insert_inst(LoadImmediate, {value_reg, ret_value_location});
        } else if (is_a<PhysicalRegister>(ret_value_location)) {
            if (ret_value_location != value_reg)
                move_same_type(value_reg, ret_value_location);
        } else if (is_a<StackObject>(ret_value_location)) {
            auto stack_object = as_a<StackObject>(ret_value_location);
            assert(stack_object->get_type() == func->get_ret_type());
            auto load_op = stack_object->load_op();
            auto offset = Offset2int(func->get_frame().offset.at(stack_object));
            safe_load_store(load_op, value_reg, offset, t0);
        } else
            throw unreachable_error{};
    }
    insert_inst(Jump, {exit});
    link(label, exit);
    label->get_insts().erase(inst);
}

CodeGen::ArgInfo CodeGen::split_func_args_call_ver() const {
    auto call = _upgrade_context.inst;
    struct ArgInfo arg_info;
    unsigned i_idx = 0, f_idx = 0;
    for (unsigned i = (call->will_write_register() ? 2 : 1);
         i < call->get_operand_num(); ++i) {
        auto arg = call->get_operand(i);
        bool is_float;
        decltype(ArgInfo::_info::location) location;
        if (is_a<MemObject>(arg)) {
            auto stack_object = as_a<StackObject>(arg);
            is_float = stack_object->is_float_usage();
            location = stack_object;
        } else if (is_a<PhysicalRegister>(arg)) {
            auto preg = as_a<PhysicalRegister>(arg);
            is_float = is_a<FPReg>(arg);
            location = preg;
        } else if (is_a<Immediate>(arg)) {
            is_float = is_a<FImm32bit>(arg);
            location = as_a<Immediate>(arg);
        } else
            throw unreachable_error{"wrong case after step1"};
        auto &info_arr =
            is_float ? arg_info.float_args_in_reg : arg_info.int_args_in_reg;
        auto &idx = is_float ? f_idx : i_idx;
        ArgInfo::_info _info{true, is_float, location};
        if (idx < 8)
            info_arr.at(idx++) = _info;
        else
            arg_info.args_in_stack.push_back(_info);
    }
    return arg_info;
}

CodeGen::StackPassResult CodeGen::pass_args_stack(const ArgInfo &arg_info,
                                                  Offset stack_grow_size1) {
    // the stack grow size because of arg-pass on stack
    Offset stack_grow_size2 =
        SP_ALIGN(arg_info.args_in_stack.size() * TARGET_MACHINE_SIZE);
    map<PhysicalRegister *, bool> changed = {
        {t0, false}, {t1, false}, {ft0, false}};
    if (stack_grow_size2 == 0)
        return {0, changed};
    /* stack grow, free temp regsiter: t0, t1 and ft0
     * t0 and ft0 are used to load value, t1 is used to represent offset */
    safe_load_store(SD, t0, tmp_arg_off.at(t0), nullptr);
    if (stack_change(-Offset2int(stack_grow_size2), t0))
        safe_load_store(LD, t0,
                        Offset2int(stack_grow_size2) + tmp_arg_off.at(t0), t0);
    safe_load_store(SD, t0, tmp_arg_off.at(t0), nullptr);
    safe_load_store(SD, t1, tmp_arg_off.at(t1), nullptr);
    safe_load_store(FSW, ft0, tmp_arg_off.at(ft0), nullptr);

    for (unsigned i = 0; i < arg_info.args_in_stack.size(); ++i) {
        auto info = arg_info.args_in_stack.at(i);
        auto load_op = info.is_float ? FLW : LD;
        auto store_op = info.is_float ? FSW : SD;
        IPReg *tmp_addr_reg = nullptr;

        PhysicalRegister *value_reg = nullptr;
        if (holds_alternative<PhysicalRegister *>(info.location)) {
            value_reg = get<PhysicalRegister *>(info.location);
            // t0, t1 and ft0 may be overwriten
            if (contains(changed, value_reg) and changed.at(value_reg)) {
                safe_load_store(load_op, value_reg, tmp_arg_off.at(value_reg),
                                nullptr);
                changed.at(value_reg) = false;
            }
            if (value_reg == t1)
                tmp_addr_reg = t0;
            else
                tmp_addr_reg = t1;
        } else if (holds_alternative<Immediate *>(info.location)) {
            // TODO: test with func with a lot of float param and remove this
            // assert if the result is correct
            assert(not info.is_float); // FIXME resolve float immediate
            insert_inst(LoadImmediate, {t0, get<Immediate *>(info.location)});
            value_reg = t0;
            tmp_addr_reg = t1;
            changed.at(t0) = true;
        } else if (holds_alternative<StackObject *>(info.location)) {
            value_reg =
                info.is_float ? ft0 : static_cast<PhysicalRegister *>(t0);
            tmp_addr_reg = t1;
            changed.at(t1) |= distinguish_stack_usage(
                value_reg, get<StackObject *>(info.location), t1,
                stack_grow_size1 + stack_grow_size2);
            changed.at(value_reg) = true;
        } else
            throw unreachable_error{};
        changed.at(tmp_addr_reg) |=
            safe_load_store(store_op, value_reg,
                            Offset2int(i * TARGET_MACHINE_SIZE), tmp_addr_reg);
    }
    return {stack_grow_size2, changed};
}

void CodeGen::pass_args_in_reg(const ArgInfo &arg_info, Offset stack_grow_size1,
                               StackPassResult res, bool for_float) {
    auto &location_info =
        for_float ? arg_info.float_args_in_reg : arg_info.int_args_in_reg;

    const int ARG_REGS = 8;

    class OrderParser {
        using NeedByVec = vector<unsigned>;
        using AssignOrder = vector<unsigned>;

      private:
        // ai is need by a_k, k is in neeby[i]
        array<NeedByVec, ARG_REGS> needby;
        // if ai has been checked, to detect cycle
        array<bool, ARG_REGS> vis{};
        deque<unsigned> queue;

        const decltype(ArgInfo::int_args_in_reg) &location_info;
        const StackPassResult &res;

      public:
        unsigned arg_cnt;
        AssignOrder order;
        array<bool, ARG_REGS> backup{};      // if ai need a backup
        vector<unsigned> value_in_tmp_first; // assign args use tmp reg first

        explicit OrderParser(decltype(location_info) l, decltype(res) r)
            : location_info(l), res(r) {
            gen_rely_graph();
            gen_order();
        }

      private:
        void gen_rely_graph() {
            unsigned arg_idx;
            // generate rely links
            for (arg_idx = 0; arg_idx < ARG_REGS; ++arg_idx) {
                auto info = location_info.at(arg_idx);
                if (not info.valid)
                    break;
                if (not holds_alternative<PhysicalRegister *>(info.location))
                    continue;
                auto preg = get<PhysicalRegister *>(info.location);
                if (preg->is_arg_reg() and preg->get_arg_idx() != arg_idx) {
                    // rely value in a a{} and is not self
                    needby.at(preg->get_arg_idx()).push_back(arg_idx);
                }
                // assign values that is in t0, t1 or ft0 first!
                if (contains(res.changed, preg))
                    value_in_tmp_first.push_back(arg_idx);
            }

            arg_cnt = arg_idx;
        }

        void gen_order() {
            auto is_valid_arg_idx = [&](unsigned i) {
                return i < arg_cnt and not contains(value_in_tmp_first, i);
            };
            for (unsigned i = 0; i < arg_cnt; ++i)
                if (is_valid_arg_idx(i))
                    queue.push_back(i); // initialize
            while (not queue.empty()) {
                auto i = queue.front();
                queue.pop_front();
                if (contains(order, i))
                    continue; // a_i has been assigned order
                bool assign = false;
                if (needby.at(i).size() == 0) {
                    // there is no other argument relying on a_i
                    assign = true;
                } else if (vis.at(i)) { // cycle detected
                    backup[i] = true;
                    assign = true;
                } else
                    assign = false;

                if (assign) {
                    order.push_back(i);
                    for (unsigned j = 0; j < arg_cnt; ++j) {
                        // eliminate rely links
                        auto &need_vec = needby.at(j);
                        auto iter = find(need_vec.begin(), need_vec.end(), i);
                        if (iter != need_vec.end()) {
                            need_vec.erase(iter);
                            if (need_vec.size() == 0 and is_valid_arg_idx(j))
                                queue.push_front(j);
                            break; // a_i relys on 1 a{} at most
                        }
                    }
                } else
                    queue.push_back(i);

                vis[i] = true;
            }
        }
    };

    OrderParser order_parser(location_info, res);
    const auto &value_in_tmp_first = order_parser.value_in_tmp_first;
    const auto &order = order_parser.order;
    const auto &backup = order_parser.backup;

    const auto move_op = for_float ? FMove : Move;
    const auto load_op = for_float ? FLW : LD; // FIXME LD or LW?

    /* 2. pass arg */
    PhysicalRegister *target_reg;
    assert(value_in_tmp_first.size() + order.size() == order_parser.arg_cnt);
    // args relying on t0/t1/ft0
    for (auto i : value_in_tmp_first) {
        target_reg = preg_mgr.get_arg_reg(i, for_float);
        auto preg = get<PhysicalRegister *>(location_info.at(i).location);
        if (res.changed.at(preg)) {
            safe_load_store(load_op, preg, tmp_arg_off.at(preg), nullptr);
            res.changed.at(preg) = false;
        }
        move_same_type(target_reg, preg);
    }

    const auto backup_op = for_float ? FMVXW : move_op;
    const auto recvoer_op = for_float ? FMVWX : move_op;
    // the other args
    // the overloaded function below need caller set target_reg
    auto ArgValueParser = overloaded{
        [&](StackObject *stack_object) {
            auto add_off = stack_grow_size1 + res.stack_grow_size2;
            distinguish_stack_usage(target_reg, stack_object, t0, add_off);
        },
        [&](PhysicalRegister *preg) {
            insert_inst(COMMENT,
                        {target_reg, value_mgr.create<Comment>("="), preg});
            assert(not contains(tmp_arg_off, preg));
            if (backup.at(target_reg->get_arg_idx()))
                insert_inst(backup_op, {t1, target_reg});
            if (preg->is_arg_reg() and backup.at(preg->get_arg_idx()))
                insert_inst(recvoer_op, {target_reg, t1});
            else
                move_same_type(target_reg, preg);
        },
        [&](mir::Immediate *imm) {
            if (is_a<FImm32bit>(imm)) {
                insert_inst(LoadImmediate, {t0, imm});
                insert_inst(FMVWX, {target_reg, t0});
            } else {
                insert_inst(LoadImmediate, {target_reg, imm});
            }
        },
    };
    for (auto i : order) {
        auto info = location_info.at(i);
        target_reg = preg_mgr.get_arg_reg(i, for_float);
        visit(ArgValueParser, info.location);
    }
}

// pass arguments and caller saves
void CodeGen::resolve_call() {
    auto func = _upgrade_context.func;
    auto &location = func->get_frame().offset;
    auto inst = _upgrade_context.inst;

    // pre analysis for the inst
    Value *ret_location = nullptr;
    Function *called_func;
    if (inst->will_write_register()) {
        ret_location = inst->get_operand(0);
        called_func = as_a<Function>(inst->get_operand(1));
    } else {
        called_func = as_a<Function>(inst->get_operand(0));
    }

    auto critical_iregs = current_critical_regs(false, Saver::Caller);
    auto critical_fregs = current_critical_regs(true, Saver::Caller);
    if (is_a<IPReg>(ret_location))
        critical_iregs.erase(as_a<IPReg>(ret_location));
    else if (is_a<FPReg>(ret_location))
        critical_fregs.erase(as_a<FPReg>(ret_location));
    // clang-format off
    const Offset stack_grow_size1 = ALIGN(
        critical_iregs.size()*TARGET_MACHINE_SIZE + critical_fregs.size()*BASIC_TYPE_SIZE,
        SP_ALIGNMENT
    );

    // clang-format on
    auto resolve_caller_save = [&](bool before_call) {
        if (before_call) // alloc stack space
            stack_change(-int(stack_grow_size1), nullptr);
        MIR_INST int_op = before_call ? SD : LD;
        MIR_INST float_op = before_call ? FSW : FLW;
        Offset off = 0;
        for (auto ireg : critical_iregs) { // int registers
            insert_inst(int_op, {ireg, create_imm(off), sp});
            off += TARGET_MACHINE_SIZE;
        }
        for (auto freg : critical_fregs) { // float registers
            insert_inst(float_op, {freg, create_imm(off), sp});
            off += BASIC_TYPE_SIZE;
        }
        if (not before_call) // recover sp
            stack_change(stack_grow_size1, nullptr);
        assert(ALIGN(off, SP_ALIGNMENT) == stack_grow_size1);
    };

    auto arg_info = split_func_args_call_ver();

    comment("caller saves before call");
    resolve_caller_save(true);

    comment("pass args through stack");
    auto stack_res = pass_args_stack(arg_info, stack_grow_size1);

    comment("pass int args through reg");
    pass_args_in_reg(arg_info, stack_grow_size1, stack_res, false);

    comment("pass float args through reg");
    pass_args_in_reg(arg_info, stack_grow_size1, stack_res, true);

    insert_inst(Call, {called_func});
    stack_change(Offset2int(stack_res.stack_grow_size2), t0);
    if (ret_location) {
        if (is_a<PhysicalRegister>(ret_location))
            move_same_type(ret_location, preg_mgr.get_ret_val_reg(
                                             0, is_a<FPReg>(ret_location)));
        else if (is_a<StackObject>(ret_location)) {
            auto stack_object = as_a<StackObject>(ret_location);
            auto off = stack_grow_size1 + location.at(stack_object);
            safe_load_store(
                stack_object->store_op(),
                preg_mgr.get_ret_val_reg(0, stack_object->is_float_usage()),
                Offset2int(off), t0);

        } else
            throw unreachable_error{"bad return value location"};
    }
    resolve_caller_save(false);

    inst->degenerate_to_comment();
}

void CodeGen::resolve_move() {
    auto inst = _upgrade_context.inst;
    auto func = _upgrade_context.func;
    auto &frame_location = func->get_frame().offset;

    assert(inst->get_operand_num() == 2);
    auto dest = inst->get_operand(0);
    auto src = inst->get_operand(1);

    if (not is_a<StackObject>(dest) and not is_a<StackObject>(src))
        return;
    if (is_a<StackObject>(dest) and is_a<StackObject>(src)) {
        resolve_stack();
        return;
    }

    // helper function to find 1 tmp reg
    IPReg *tmp_reg{nullptr};
    bool need_recover{false};
    auto find_tmp_reg = [&]() {
        if (is_a<IPReg>(dest))
            tmp_reg = as_a<IPReg>(dest);
        else {
            auto critical_iregs = current_critical_regs(false, Saver::ALL);
            auto writable_iregs = preg_mgr.get_all_regs_writable(false);
            vector<PhysicalRegister *> tmp_regs;
            // src cannot be overwriten
            if (is_a<PhysicalRegister>(src))
                writable_iregs.erase(as_a<PhysicalRegister>(src));

            set_difference(writable_iregs.begin(), writable_iregs.end(),
                           critical_iregs.begin(), critical_iregs.end(),
                           back_inserter(tmp_regs));
            if (tmp_regs.size())
                tmp_reg = as_a<IPReg>(tmp_regs[0]);
        }
        if (tmp_reg == nullptr) {
            tmp_reg = t0;
            need_recover = true;
            safe_load_store(SD, t0, tmp_arg_off.at(t0), nullptr);
        }
    };

    if (is_a<StackObject>(dest)) {
        auto stack_object = as_a<StackObject>(dest);
        int off = Offset2int(frame_location.at(stack_object));
        if (not Imm12bit::check_in_range(off))
            find_tmp_reg();
        safe_load_store(stack_object->store_op(), as_a<PhysicalRegister>(src),
                        off, tmp_reg);
    } else {
        auto stack_object = as_a<StackObject>(src);
        int off = Offset2int(frame_location.at(stack_object));

        if (not Imm12bit::check_in_range(off))
            find_tmp_reg();

        safe_load_store(stack_object->load_op(), as_a<PhysicalRegister>(dest),
                        off, tmp_reg);
    }

    if (need_recover)
        safe_load_store(LD, tmp_reg, tmp_arg_off.at(tmp_reg), nullptr);
    inst->degenerate_to_comment();
}

/* @brief expand the instruction with stack object to lw/sw instructions
 *
 * =================HOW-TO-DO=================
 * A general case of inst got from stage2.step1 is:
 *      `op location1, location2, location3, ...`
 * - If location_i is a register(physical more specifically), nothing to do
 * - If the location_i is a StackObject, we should take care of it:
 *   - src case: load it into a temp register
 *   - dest case: only occurs on location1, we write back into the position
 * - For StackObject, it may be used as either its value or its address, the
 *   function `distinguish_stack_usage()` will handle that
 * - Must be handled carefully:
 *   1. backup for temp registers if in use(except for dest of cur inst)
 *   2. when choosing temp registers, we should not make conflict with existing
 *      reg in the inst
 *   3. if address will overflow, use another temp regsiter(int)
 *   4. if inst is a branch inst, recover tmp regs carefully
 */
void CodeGen::resolve_stack() {
    auto func = _upgrade_context.func;
    auto label = _upgrade_context.label;
    auto inst = _upgrade_context.inst;
    auto &frame_location = func->get_frame().offset;

    // 0. parse this instruction
    struct {
        bool rd_exist{false};
        Value *rd_location{nullptr};
        optional<StackObject *> as_stack_object;

        void set(Value *loc) {
            rd_exist = true;
            rd_location = loc;
            if (is_a<StackObject>(loc)) {
                as_stack_object = as_a<StackObject>(loc);
            }
        }

    } rd_info; // dest reg info
    if (inst->will_write_register())
        rd_info.set(inst->get_operand(0));

    // 1. filter out the values that is not StackObject
    set<PhysicalRegister *> tmp_regs_in_use, tmp_regs_save_if_critical;
    vector<StackObject *> stack_objects;
    for (unsigned i = (rd_info.rd_exist ? 1 : 0); i < inst->get_operand_num();
         ++i) {
        auto op = inst->get_operand(i);
        if (is_a<PhysicalRegister>(op)) {
            auto preg = as_a<PhysicalRegister>(op);
            if (preg->is_temp_reg())
                tmp_regs_in_use.insert(preg);
        } else if (is_a<StackObject>(op)) {
            stack_objects.push_back(as_a<StackObject>(op));
        }
    }

    // 2. allocate a temp regsiter for each StackObject
    deque<StackObject *> load_order; // order: floats then ints
    map<StackObject *, PhysicalRegister *> tmp_reg_map;
    unsigned i_idx = 0, f_idx = 0;
    // TODO:
    // - select rd as tmp reg first as we do not need to save rd, though it
    // could be critical
    // - try selecting a reg that is not critical
    auto find_tmp_reg = [&](bool for_float) {
        auto &idx = for_float ? f_idx : i_idx;
        PhysicalRegister *tmp_reg = preg_mgr.get_temp_reg(idx, for_float);
        while (contains(tmp_regs_in_use, tmp_reg))
            tmp_reg = preg_mgr.get_temp_reg(++idx, for_float);
        tmp_regs_in_use.insert(tmp_reg);
        return tmp_reg;
    };
    for (auto stack_object : stack_objects) {
        bool is_float = stack_object->is_float_usage();
        PhysicalRegister *tmp_reg = find_tmp_reg(is_float);
        tmp_regs_save_if_critical.insert(tmp_reg);
        tmp_reg_map.insert({stack_object, tmp_reg});
        if (is_float)
            load_order.push_front(stack_object);
        else
            load_order.push_back(stack_object);
    }

    // 3. resolve dest reg: tmp_reg can be reused
    if (rd_info.rd_exist and rd_info.as_stack_object.has_value()) {
        auto stack_object = rd_info.as_stack_object.value();
        auto is_float = stack_object->is_float_usage();
        auto &idx = is_float ? f_idx : i_idx;
        if (idx == 0) { // no same type tmp-reg here
            auto tmp_reg = find_tmp_reg(is_float);
            tmp_regs_save_if_critical.insert(tmp_reg);
            tmp_reg_map.insert({stack_object, tmp_reg});
        } else { // there is at least 1 tmp-reg with same type
            PhysicalRegister *reused_tmp_reg;
            if (is_float)
                reused_tmp_reg = tmp_reg_map.at(load_order.front());
            else
                reused_tmp_reg = tmp_reg_map.at(load_order.back());
            tmp_reg_map.insert({stack_object, reused_tmp_reg});
        }
    }

    if (tmp_reg_map.empty()) // no stack object in the inst
        return;

    // 4. parse critical regs(wipe off the dest reg), which are to be saved
    auto critical_iregs = current_critical_regs(false, Saver::ALL);
    auto critical_fregs = current_critical_regs(true, Saver::ALL);
    decltype(tmp_regs_save_if_critical) critical_tmp_iregs, critical_tmp_fregs;
    set_intersection(tmp_regs_save_if_critical.begin(),
                     tmp_regs_save_if_critical.end(), critical_iregs.begin(),
                     critical_iregs.end(),
                     inserter(critical_tmp_iregs, critical_tmp_iregs.begin()));
    set_intersection(tmp_regs_save_if_critical.begin(),
                     tmp_regs_save_if_critical.end(), critical_fregs.begin(),
                     critical_fregs.end(),
                     inserter(critical_tmp_fregs, critical_tmp_fregs.begin()));
    if (rd_info.rd_exist and is_a<PhysicalRegister>(rd_info.rd_location)) {
        // wipe off the dest reg, because it will be overwriten right away
        auto preg = as_a<PhysicalRegister>(rd_info.rd_location);
        critical_tmp_iregs.erase(preg);
        critical_tmp_fregs.erase(preg);
    }
    Offset stack_grow_size = critical_tmp_iregs.size() * TARGET_MACHINE_SIZE +
                             critical_tmp_fregs.size() * BASIC_TYPE_SIZE;

    // 5. resolve imm overflow on stack offset
    IPReg *tmp_addr_reg = nullptr;
    Offset max_off = 0;
    for (auto [stack_object, _] : tmp_reg_map)
        max_off = max(max_off, frame_location.at(stack_object));
    if (not Imm12bit::check_in_range(
            Offset2int(SP_ALIGN(stack_grow_size) + max_off))) {
        // the offset will overflow on 12bit imm
        bool need_new_temp_reg = false;
        if (rd_info.as_stack_object.has_value()) {
            auto rd_offset = frame_location.at(rd_info.as_stack_object.value());
            if (not Imm12bit::check_in_range(
                    Offset2int(SP_ALIGN(stack_grow_size) + rd_offset)))
                need_new_temp_reg = true;
        } else if (i_idx == 0)
            need_new_temp_reg = true;
        tmp_addr_reg =
            as_a<IPReg>(need_new_temp_reg ? find_tmp_reg(false)
                                          : tmp_reg_map.at(load_order.back()));
    }
    if (tmp_addr_reg and
        contains(critical_iregs, static_cast<PhysicalRegister *>(tmp_addr_reg)))
        critical_tmp_iregs.insert(tmp_addr_reg);
    stack_grow_size = SP_ALIGN(critical_tmp_iregs.size() * TARGET_MACHINE_SIZE +
                               critical_tmp_fregs.size() * BASIC_TYPE_SIZE);

    // 6. generate insts
    Label *new_label = nullptr;
    Instruction *insert_before = nullptr;
    auto safe_branch_prepare = [&]() {
        if (not inst->is_branch_inst())
            return;
        if (stack_grow_size == 0)
            return; // no tmp regs need to recover
        // assert branch has format: op r1, r2, label
        auto target_label = as_a<Label>(inst->operands().at(2));
        auto new_label_name =
            "Interim_" + label->get_name() + "_" + target_label->get_name();
        new_label = func->add_label(new_label_name);
        // after recover tmp regs, jump to the target label
        insert_before = &new_label->add_inst(Jump, {target_label});
        // maintain prev/succ
        unlink(label, target_label);
        link(label, new_label);
        link(new_label, target_label);
        // update context
        _upgrade_context.new_labels.insert(new_label);
    };
    auto tmp_reg_backup_and_recover = [&](bool backup) {
        auto recover = not backup;
        MIR_INST i_mem_op = backup ? SD : LD;
        MIR_INST f_mem_op = backup ? FSW : FLW;
        if (backup)
            stack_change(-Offset2int(stack_grow_size), nullptr);
        int off = 0;
        for (auto ireg : critical_tmp_iregs) {
            safe_load_store(i_mem_op, ireg, off, nullptr);
            if (recover and new_label)
                new_label->insert_before(insert_before, LD,
                                         {ireg, create_imm(off), sp});
            off += TARGET_MACHINE_SIZE;
        }
        for (auto freg : critical_tmp_fregs) {
            safe_load_store(f_mem_op, freg, off, nullptr);
            if (recover and new_label)
                new_label->insert_before(insert_before, FLW,
                                         {freg, create_imm(off), sp});
            off += TARGET_MACHINE_SIZE;
        }
        if (recover) {
            auto delta = Offset2int(stack_grow_size);
            stack_change(delta, nullptr);
            if (new_label)
                new_label->insert_before(insert_before, ADDI,
                                         {sp, sp, create_imm(delta)});
        }
    };
    auto get_value_from_stack = [&]() {
        for (auto stack_object : load_order) {
            auto tmp_reg = tmp_reg_map.at(stack_object);
            distinguish_stack_usage(tmp_reg, stack_object, tmp_addr_reg,
                                    stack_grow_size);
        }
    };
    auto generate_same_inst = [&]() {
        vector<Value *> new_operands;
        for (auto op : inst->operands()) {
            if (is_a<StackObject>(op)) {
                auto stack_object = as_a<StackObject>(op);
                new_operands.push_back(tmp_reg_map.at((stack_object)));
            } else
                new_operands.push_back(op);
        }
        if (new_label)
            new_operands[2] = new_label;
        insert_inst(inst->get_opcode(), new_operands);
        if (rd_info.rd_exist and rd_info.as_stack_object.has_value()) {
            auto stack_object = rd_info.as_stack_object.value();
            auto off = stack_grow_size + frame_location.at(stack_object);
            safe_load_store(stack_object->store_op(),
                            tmp_reg_map.at(stack_object), Offset2int(off),
                            tmp_addr_reg);
        }
    };

    // instruction sequence start
    safe_branch_prepare();
    tmp_reg_backup_and_recover(true);
    get_value_from_stack();
    generate_same_inst();
    tmp_reg_backup_and_recover(false);
    inst->degenerate_to_comment();
}
