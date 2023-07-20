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
#include <functional>
#include <stack>
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
using Reason = StatckObject::Reason;
using RegIDType = Register::RegIDType;

// TODO
// - IMM 12bit overflow

auto &preg_mgr = PhysicalRegisterManager::get();
auto &value_mgr = ValueManager::get();
auto sp = preg_mgr.sp();

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

void CodeGen::safe_load_store(MIR_INST op, PhysicalRegister *rd, int offset,
                              IPReg *temp_reg, mir::Label *label) {
    auto gen_inst = [&](MIR_INST _op, vector<Value *> operands) {
        if (label)
            label->add_inst(_op, operands);
        else
            insert_inst(_op, operands);
    };
    if (Imm12bit::check_in_range(offset)) {
        gen_inst(op, {rd, create_imm(offset), sp});
    } else {
        gen_inst(LoadImmediate, {temp_reg, create_imm(offset, 32)});
        gen_inst(ADD, {temp_reg, temp_reg, sp});
        gen_inst(op, {rd, create_imm(0), temp_reg});
    }
}

CodeGen::ArgInfo CodeGen::split_func_args() const {
    auto func = _upgrade_context.func;
    auto &arg_spilled_location = _upgrade_context.arg_spilled_location;
    auto &ireg_map = _allocator.get_reg_map(func, false);
    auto &freg_map = _allocator.get_reg_map(func, true);
    auto &frame = func->get_frame();
    ArgInfo info;
    unsigned i_idx = 0, f_idx = 0;
    for (auto arg : func->get_args()) {
        bool is_float = is_a<FVReg>(arg);
        auto &reg_map = is_float ? freg_map : ireg_map;
        auto &info_arr =
            is_float ? info.float_args_in_reg : info.int_args_in_reg;
        auto &idx = is_float ? f_idx : i_idx;

        decltype(ArgInfo::_info::location) location;
        if (contains(arg_spilled_location, arg))
            location = frame.offset.at(arg_spilled_location.at(arg));
        else {
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
    auto t0 = preg_mgr.temp(0);
    auto t1 = preg_mgr.temp(1);
    auto ft0 = preg_mgr.ftemp(0);

    /* for first 8 args, have convetion_reg in calling convetion
     * - if has a physical reg `r`, move into a{} into `r`
     * - if spilled during reg-allocation, store a{} into mem */
    auto ArgResolver = overloaded{
        // arg is allocated to stack during reg allocation
        [&](Offset off) {
            MIR_INST store_op;
            if (is_a<FPReg>(convetion_reg)) {
                throw not_implemented_error{};
                /* store_op = FSW */
            } else {
                store_op = SD; // FIXME is this right???
            }
            // NOTE: use t1 as temp here, t0 may be used on stack-pass case
            safe_load_store(store_op, convetion_reg, Offset2int(off), t1,
                            entry);
        },
        // arg is allocated a physical reg during reg allocation
        [&](PhysicalRegister *preg) {
            MIR_INST move_op;
            if (is_a<FPReg>(convetion_reg)) {
                throw not_implemented_error{};
                // move_op = FMV;
            } else {
                move_op = Move;
            }
            if (preg != convetion_reg)
                entry->add_inst(move_op, {preg, convetion_reg});
        },
    };
    auto arg_info = split_func_args();
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
    convetion_reg = preg_mgr.temp(0);
    auto frame_size = func->get_frame().size;
    // arguments passed on stack
    for (unsigned i = 0; i < arg_info.args_in_stack.size(); ++i) {
        auto stack_arg_info = arg_info.args_in_stack.at(i);
        auto off = (frame_size + i * BASIC_TYPE_SIZE);
        MIR_INST load_op;
        if (stack_arg_info.is_float) {
            throw not_implemented_error{};
            convetion_reg = ft0;
            /* store_op = FLW */
        } else {
            convetion_reg = t0;
            load_op = LD; // FIXME is this right???
        }
        safe_load_store(load_op, convetion_reg, Offset2int(off), t1, entry);
        visit(ArgResolver, stack_arg_info.location);
    }
}

// TODO resolve arguments
// resolve prologue and epilogue
// -
void CodeGen::resolve_logue() {
    auto func = _upgrade_context.func;
    auto entry = _upgrade_context.entry;
    auto exit = _upgrade_context.exit;
    auto &frame = func->get_frame();
    int frame_size = frame.size;
    // in the logue case, we can use t0 regardless of inner data
    auto t0 = preg_mgr.temp(0);
    // sp move, to make sure imm in range
    auto stack_change = [&](Label *label, int offset) {
        if (Imm12bit::check_in_range(offset)) {
            label->add_inst(ADDI, {sp, sp, create_imm(offset)});
        } else {
            label->add_inst(LoadImmediate, {t0, create_imm(offset, 32)});
            label->add_inst(ADD, {sp, sp, t0});
        }
    };

    // check type cast overflow
    assert(frame_size >= 0 and frame_size == frame.size);

    // no frame relation need to maintain
    if (frame_size == 0) {
        exit->add_inst(Ret, {});
        return;
    }

    /* prologue */
    stack_change(entry, -frame_size); // stack grow
    // save callee regs
    for (auto save : func->get_callee_saves()) {
        auto regid = save->saved_reg_id();
        auto off = Offset2int(frame.offset.at(save));
        MIR_INST store_op;
        PhysicalRegister *src_reg;
        if (save->is_float_reg()) {
            // store_op = FSW;
            // reg = preg_mgr.get_float_reg(regid);
            throw not_implemented_error{};
        } else {
            store_op = SD;
            src_reg = preg_mgr.get_int_reg(regid);
        }
        safe_load_store(store_op, src_reg, off, t0, entry);
    }
    // resolve args
    coordinate_func_args();

    // fp: we do not use it for now
    // entry->add_inst(ADDI, {preg_mgr.fp(), sp, create_imm(frame_size)});

    /* epilogue */
    for (auto save : func->get_callee_saves()) {
        auto regid = save->saved_reg_id();
        auto off = Offset2int(frame.offset.at(save));
        MIR_INST load_op;
        PhysicalRegister *dest_reg;
        if (save->is_float_reg()) {
            // store_op = FLW;
            // dest_reg = preg_mgr.get_float_reg(regid);
            throw not_implemented_error{};
        } else {
            load_op = LD;
            dest_reg = preg_mgr.get_int_reg(regid);
        }
        safe_load_store(load_op, dest_reg, off, t0, exit);
    }
    // stack change
    stack_change(exit, frame_size);
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
    } break;
    case Stage::stage2:
        os << "# stage2, complete asm\n";
        break;
    }

    Context context{c._stage, Role::Full, c._allocator};

    c._mir_module->dump(os, context);
    return os;
}

set<RegIDType> CodeGen::current_critical_regs(bool want_float) const {
    auto func = _upgrade_context.func;
    auto inst = _upgrade_context.inst;

    set<RegIDType> critical_regs;

    auto instid = _allocator.get_cfg_info(func).instid.at(inst);
    auto &out_set =
        _allocator.get_liveness(func, want_float).at(OUT_POINT(instid));
    auto &reg_map = _allocator.get_reg_map(func, want_float);
    for (auto vreg : out_set) {
        if (contains(reg_map, vreg))
            critical_regs.insert(reg_map.at(vreg).reg->get_id());
    }

    return critical_regs;
}

void CodeGen::upgrade_step1() {
    auto func = _upgrade_context.func;

    auto &int_reg_map = _allocator.get_reg_map(func, false);
    auto &int_spilled = _allocator.get_spilled(func, false);
    auto &float_reg_map = _allocator.get_reg_map(func, true);
    auto &float_spilled = _allocator.get_spilled(func, true);
    map<RegIDType, StatckObject *> int_spilled_location, float_spilled_location;
    set<RegIDType> int_callee_saves, float_callee_saves;

    // spilled virtual reg to stack
    for (auto spill_id : int_spilled) {
        // FIXME use more specific size(only pointer use 8 bytes)
        int_spilled_location.insert(
            {spill_id,
             func->add_local_var(BasicType::INT, TARGET_MACHINE_SIZE,
                                 TARGET_MACHINE_SIZE, Reason::Spilled)});
    }
    for (auto spill_id : float_spilled) {
        // FIXME use more specific size(only pointer use 8 bytes)
        float_spilled_location.insert(
            {spill_id,
             func->add_local_var(BasicType::FLOAT, TARGET_MACHINE_SIZE,
                                 TARGET_MACHINE_SIZE, Reason::Spilled)});
    }

    // 1st pass
    for (auto label : func->get_labels())
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
            for (unsigned i = 0; i < inst.get_operand_num(); ++i) {
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

    // NOTE we may save all callee saves whether or not cur func uses them
    for (auto preg_id : int_callee_saves)
        func->add_callee_save(BasicType::INT, preg_id);
    for (auto preg_id : float_callee_saves)
        func->add_callee_save(BasicType::FLOAT, preg_id);

    // update context: argument spill info
    for (auto arg : func->get_args()) {
        if (not contains(int_spilled, arg->get_id()))
            continue;
        decltype(int_spilled_location) &spilled_location =
            is_a<IVReg>(arg) ? int_spilled_location : float_spilled_location;
        _upgrade_context.arg_spilled_location.insert(
            {arg, spilled_location.at(arg->get_id())});
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

            switch (inst->get_opcode()) {
            case Ret:
                resolve_ret();
                break;
            case Call:
                resolve_call();
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
        PhysicalRegister *value_reg;
        MIR_INST load_op;
        switch (func->get_ret_type()) {
        case BasicType::VOID:
            throw unreachable_error{};
        case BasicType::INT:
            value_reg = preg_mgr.a(0);
            load_op = LW;
            break;
        case BasicType::FLOAT:
            /* value_reg = preg_mgr.fa(0);
             * load_op = FLW; */
            throw not_implemented_error{};
            break;
        }
        // return value
        auto ret_value = inst->get_operand(0);
        if (is_a<Immediate>(ret_value)) {
            insert_inst(LoadImmediate, {value_reg, ret_value});
        } else if (is_a<PhysicalRegister>(ret_value)) {
            if (ret_value != value_reg)
                insert_inst(Move, {value_reg, ret_value});
        } else if (is_a<StatckObject>(ret_value)) {
            auto stack_object = as_a<StatckObject>(ret_value);
            assert(stack_object->get_type() == func->get_ret_type());
            auto offset = Offset2int(func->get_frame().offset.at(stack_object));
            safe_load_store(load_op, value_reg, offset, preg_mgr.temp(0));
        } else
            throw unreachable_error{};
    }
    insert_inst(Jump, {exit});
    label->get_insts().erase(inst);
}

//
// pass arguments and caller saves
void CodeGen::resolve_call() { // TODO
    return;
    auto func = _upgrade_context.func;
    auto label = _upgrade_context.label;
    auto inst = _upgrade_context.inst;
    auto location = func->get_frame().offset;

    //
    auto critical_iregs = current_critical_regs(false);
    auto critical_fregs = current_critical_regs(true);

    struct StackResolver {
        unsigned t_id;
        Offset location;
    };
    map<Value *, StackResolver> core;
    unsigned t_id = 0; // use register t{t_id}
    Offset max_offset = 0;

    // give each object a temp register to fill in
    for (unsigned i = 0; i < inst->get_operand_num(); ++i) {
        auto op = inst->get_operand(i);
        if (not is_a<StatckObject>(op))
            continue;
        auto offset = location.at(as_a<StatckObject>(op));
        max_offset = max(offset, max_offset);
        core.insert({op, {t_id++, offset}});
    }
    if (t_id == 0) // no stack object here
        return;

    Offset stack_grow_size = t_id * TARGET_MACHINE_SIZE;

    // if overflow, use a new temp reg to express the address
    IPReg *tmp_addr_reg = nullptr;
    if (not Imm12bit::check_in_range(stack_grow_size + max_offset)) {
        t_id++;
        stack_grow_size += TARGET_MACHINE_SIZE;
        tmp_addr_reg = preg_mgr.temp(t_id - 1);
    }

    // backup
    insert_inst(ADDI, {sp, sp, create_imm(-int(stack_grow_size))});
    for (unsigned i = 0; i < t_id; ++i) {
        Offset off = i * TARGET_MACHINE_SIZE;
        insert_inst(SD, {preg_mgr.temp(i), create_imm(off), sp});
    }

    // load from stack
    vector<Value *> new_operands;
    for (unsigned i = 0; i < inst->get_operand_num(); ++i) {
        auto op = inst->get_operand(i);
        if (not is_a<StatckObject>(op)) {
            new_operands.push_back(op);
            continue;
        }
        const auto &resolver = core.at(op);
        auto temp_value_reg = preg_mgr.temp(resolver.t_id);
        auto offset = Offset2int(stack_grow_size + resolver.location);

        safe_load_store(LD, temp_value_reg, offset, tmp_addr_reg);
        new_operands.push_back(temp_value_reg);
    }

    if (inst->is_branch_inst()) {
        auto target_label = as_a<Label>(new_operands.at(2));
        auto new_label_name =
            "Interim_" + label->get_name() + "_" + target_label->get_name();
        auto new_label = func->add_label(new_label_name);
        _upgrade_context.new_labels.insert(new_label);
        new_operands[2] = new_label;

        // recover temp regs in new label
        for (unsigned i = 0; i < t_id; ++i) {
            Offset off = i * TARGET_MACHINE_SIZE;
            new_label->add_inst(LD, {preg_mgr.temp(i), create_imm(off), sp});
        }
        new_label->add_inst(ADDI, {sp, create_imm(stack_grow_size), sp});
        new_label->add_inst(Jump, {target_label});
    }

    // new instruction with same semantic
    insert_inst(inst->get_opcode(), new_operands);

    // recover temp regs in original label
    for (unsigned i = 0; i < t_id; ++i) {
        Offset off = i * TARGET_MACHINE_SIZE;
        insert_inst(LD, {preg_mgr.temp(i), create_imm(off), sp});
    }
    insert_inst(ADDI, {sp, create_imm(stack_grow_size), sp});

    label->get_insts().erase(inst);
}

// FIXME
// - int only for now!!!!
// - alloca case
/* expand the instructions with stack object to lw/sw instructions
 *
 * Taken into account:
 * - multiple stack objects, should use at least 1 temp regs to load value from
 * stack
 * - write back case: use sw to write back to stack
 * - if imm overflow on 12bit case, use another more temp register to solve it
 *
 * How to choose register: just use t series, and backup them simply
 */
void CodeGen::resolve_stack() {
    auto func = _upgrade_context.func;
    auto label = _upgrade_context.label;
    auto inst = _upgrade_context.inst;
    auto location = func->get_frame().offset;

    struct StackResolver {
        unsigned t_id;
        Offset location;
    };
    map<Value *, StackResolver> core;
    unsigned t_id = 0; // use register t{t_id}
    Offset max_offset = 0;

    // give each object a temp register to fill in
    for (unsigned i = 0; i < inst->get_operand_num(); ++i) {
        auto op = inst->get_operand(i);
        if (not is_a<StatckObject>(op))
            continue;
        auto offset = location.at(as_a<StatckObject>(op));
        max_offset = max(offset, max_offset);
        core.insert({op, {t_id++, offset}});
    }
    if (t_id == 0) // no stack object here
        return;

    Offset stack_grow_size = t_id * TARGET_MACHINE_SIZE;

    // if overflow, use a new temp reg to express the address
    IPReg *tmp_addr_reg = nullptr;
    if (not Imm12bit::check_in_range(stack_grow_size + max_offset)) {
        t_id++;
        stack_grow_size += TARGET_MACHINE_SIZE;
        tmp_addr_reg = preg_mgr.temp(t_id - 1);
    }

    // backup
    insert_inst(ADDI, {sp, sp, create_imm(-int(stack_grow_size))});
    for (unsigned i = 0; i < t_id; ++i) {
        Offset off = i * TARGET_MACHINE_SIZE;
        insert_inst(SD, {preg_mgr.temp(i), create_imm(off), sp});
    }

    // load from stack
    vector<Value *> new_operands;
    for (unsigned i = 0; i < inst->get_operand_num(); ++i) {
        auto op = inst->get_operand(i);
        if (not is_a<StatckObject>(op)) {
            new_operands.push_back(op);
            continue;
        }
        const auto &resolver = core.at(op);
        auto temp_value_reg = preg_mgr.temp(resolver.t_id);
        auto offset = Offset2int(stack_grow_size + resolver.location);

        safe_load_store(LD, temp_value_reg, offset, tmp_addr_reg);
        new_operands.push_back(temp_value_reg);
    }

    if (inst->is_branch_inst()) {
        auto target_label = as_a<Label>(new_operands.at(2));
        auto new_label_name =
            "Interim_" + label->get_name() + "_" + target_label->get_name();
        auto new_label = func->add_label(new_label_name);
        _upgrade_context.new_labels.insert(new_label);
        new_operands[2] = new_label;

        // recover temp regs in new label
        for (unsigned i = 0; i < t_id; ++i) {
            Offset off = i * TARGET_MACHINE_SIZE;
            new_label->add_inst(LD, {preg_mgr.temp(i), create_imm(off), sp});
        }
        new_label->add_inst(ADDI, {sp, create_imm(stack_grow_size), sp});
        new_label->add_inst(Jump, {target_label});
    }

    // new instruction with same semantic
    insert_inst(inst->get_opcode(), new_operands);

    // recover temp regs in original label
    for (unsigned i = 0; i < t_id; ++i) {
        Offset off = i * TARGET_MACHINE_SIZE;
        insert_inst(LD, {preg_mgr.temp(i), create_imm(off), sp});
    }
    insert_inst(ADDI, {sp, create_imm(stack_grow_size), sp});

    label->get_insts().erase(inst);
}

void CodeGen::insert_inst(mir::MIR_INST op, std::vector<mir::Value *> vec) {
    _upgrade_context.label->insert_before(_upgrade_context.inst, op, vec);
}
