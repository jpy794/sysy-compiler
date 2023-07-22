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
// - distinguish pointer address and content in the MemObject!
// - special register for ra

auto &preg_mgr = PhysicalRegisterManager::get();
auto &value_mgr = ValueManager::get();
auto sp = preg_mgr.sp();
auto t0 = preg_mgr.temp(0);
auto t1 = preg_mgr.temp(1);
auto ft0 = preg_mgr.ftemp(0);
const std::map<mir::PhysicalRegister *, int> tmp_arg_off = {
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

Instruction *CodeGen::comment(string &&s, Label *label) {
    return gen_inst(COMMENT, {value_mgr.create<Comment>(s)}, label);
}

CodeGen::ArgInfo CodeGen::split_func_args_logue_ver() const {
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
            // NOTE: use t1 as temp here, t0 may be used on
            // stack-pass case
            safe_load_store(store_op, convetion_reg, Offset2int(off), t1,
                            entry);
        },
        // arg is allocated a physical reg during reg allocation
        [&](PhysicalRegister *preg) {
            if (preg != convetion_reg)
                move_same_type(preg, convetion_reg, entry);
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

    /* prologue */
    stack_change(-Offset2int(frame.size), t0, entry); // stack grow
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

set<RegIDType> CodeGen::current_critical_regs(bool want_float,
                                              Saver saver) const {
    auto func = _upgrade_context.func;
    auto inst = _upgrade_context.inst;

    set<RegIDType> critical_regs;

    auto instid = _allocator.get_cfg_info(func).instid.at(inst);
    auto &out_set =
        _allocator.get_liveness(func, want_float).at(OUT_POINT(instid));
    auto &reg_map = _allocator.get_reg_map(func, want_float);
    for (auto vreg : out_set) {
        if (contains(reg_map, vreg)) {
            auto preg = reg_map.at(vreg).reg;
            if (saver == preg->get_saver())
                critical_regs.insert(preg->get_id());
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

                // special case for ra:
                // - if ra is writen as a general register, it will be add in to
                // callee-saves during the above judge
                // - if ra is writen implicitly during call, the code below will
                // handle it
                if (inst.get_opcode() == Call)
                    int_callee_saves.insert(preg_mgr.ra()->get_id());
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
        } else if (is_a<StackObject>(ret_value)) {
            auto stack_object = as_a<StackObject>(ret_value);
            assert(stack_object->get_type() == func->get_ret_type());
            auto offset = Offset2int(func->get_frame().offset.at(stack_object));
            safe_load_store(load_op, value_reg, offset, preg_mgr.temp(0));
        } else
            throw unreachable_error{};
    }
    insert_inst(Jump, {exit});
    label->get_insts().erase(inst);
}

CodeGen::ArgInfo CodeGen::split_func_args_call_ver() const {
    auto func = _upgrade_context.func;
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
            is_float = stack_object->get_type() == BasicType::FLOAT;
            location = func->get_frame().offset.at(stack_object);
        } else if (is_a<PhysicalRegister>(arg)) {
            auto preg = as_a<PhysicalRegister>(arg);
            is_float = is_a<FPReg>(arg);
            location = preg;
        } else if (is_a<Immediate>(arg)) {
            is_float = false;
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
    Offset stack_grow_size2 = 0;
    for (const auto &info : arg_info.args_in_stack) {
        stack_grow_size2 +=
            info.is_float ? BASIC_TYPE_SIZE : TARGET_MACHINE_SIZE;
    }
    stack_grow_size2 = ALIGN(stack_grow_size2, SP_ALIGNMENT);
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
            assert(not info.is_float); // FIXME resolve float immediate
            insert_inst(LoadImmediate, {t0, get<Immediate *>(info.location)});
            value_reg = t0;
            tmp_addr_reg = t1;
            changed.at(t0) = true;
        } else {
            value_reg =
                info.is_float ? ft0 : static_cast<PhysicalRegister *>(t0);
            tmp_addr_reg = t1;
            Offset off = stack_grow_size1 + stack_grow_size2 +
                         get<Offset>(info.location);
            changed.at(t1) |=
                safe_load_store(load_op, value_reg, Offset2int(off), t1);
            changed.at(value_reg) = true;
        }
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
        array<bool, ARG_REGS> vis;
        deque<unsigned> queue;

        const decltype(ArgInfo::int_args_in_reg) &location_info;
        const StackPassResult &res;

      public:
        unsigned arg_cnt;
        AssignOrder order;
        array<bool, ARG_REGS> backup = {false}; // if ai need a backup
        vector<unsigned> value_in_tmp_first;    // assign args use tmp reg first

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

    auto tmp_reg = for_float ? ft0 : static_cast<PhysicalRegister *>(t0);
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
        [&](Offset off) {
            auto cur_off = off + stack_grow_size1 + res.stack_grow_size2;
            safe_load_store(load_op, tmp_reg, Offset2int(cur_off), t0);
            move_same_type(target_reg, tmp_reg);
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
        [&](mir::Immediate *imm) { // assert is not float
            insert_inst(LoadImmediate, {target_reg, imm});
        },
    };
    for (auto i : order) {
        auto info = location_info.at(i);
        target_reg = preg_mgr.get_arg_reg(i, for_float);
        visit(ArgValueParser, info.location);
        assert(not(info.is_float and
                   holds_alternative<Immediate *>(info.location)));
    }
}

// pass arguments and caller saves
void CodeGen::resolve_call() {
    auto func = _upgrade_context.func;
    auto location = func->get_frame().offset;
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
        critical_iregs.erase(as_a<IPReg>(ret_location)->get_id());
    else if (is_a<FPReg>(ret_location))
        critical_fregs.erase(as_a<FPReg>(ret_location)->get_id());
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
        MIR_INST float_op = before_call ? FSW : LW;
        Offset off = 0;
        for (auto ireg_id : critical_iregs) { // int registers
            off += TARGET_MACHINE_SIZE;
            auto ireg = preg_mgr.get_int_reg(ireg_id);
            insert_inst(int_op, {ireg, create_imm(off), sp});
        }
        for (auto freg_id : critical_fregs) { // float registers
            off += BASIC_TYPE_SIZE;
            auto freg = preg_mgr.get_float_reg(freg_id);
            insert_inst(float_op, {freg, create_imm(off), sp});
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
    // TODO return value
    stack_change(Offset2int(stack_res.stack_grow_size2), t0);
    if (ret_location) {
        if (is_a<PhysicalRegister>(ret_location))
            move_same_type(ret_location,
                           preg_mgr.ret_val(0, is_a<FPReg>(ret_location)));
        else if (is_a<StackObject>(ret_location)) {
            auto stack_object = as_a<StackObject>(ret_location);
            auto off = stack_grow_size1 + location.at(stack_object);
            bool is_float = stack_object->get_type() == BasicType::FLOAT;
            MIR_INST store_op = is_float ? FSW : SD; // FIXME
            safe_load_store(store_op,
                            preg_mgr.ret_val(0, is_a<FPReg>(ret_location)),
                            Offset2int(off), t0);

        } else
            throw unreachable_error{"bad return value location"};
    }
    resolve_caller_save(false);

    inst->degenerate_to_comment();
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
        if (not is_a<StackObject>(op))
            continue;
        auto offset = location.at(as_a<StackObject>(op));
        max_offset = max(offset, max_offset);
        core.insert({op, {t_id++, offset}});
    }
    if (t_id == 0) // no stack object here
        return;

    Offset stack_grow_size = (t_id * TARGET_MACHINE_SIZE);

    // if overflow, use a new temp reg to express the address
    IPReg *tmp_addr_reg = nullptr;
    if (not Imm12bit::check_in_range(
            ALIGN(stack_grow_size + max_offset, SP_ALIGNMENT))) {
        t_id++;
        stack_grow_size += TARGET_MACHINE_SIZE;
        tmp_addr_reg = preg_mgr.temp(t_id - 1);
    }
    stack_grow_size = ALIGN(stack_grow_size, SP_ALIGNMENT);

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
        if (not is_a<StackObject>(op)) {
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
        new_label->add_inst(ADDI, {sp, sp, create_imm(stack_grow_size)});
        new_label->add_inst(Jump, {target_label});
    }

    // new instruction with same semantic
    insert_inst(inst->get_opcode(), new_operands);

    // recover temp regs in original label
    for (unsigned i = 0; i < t_id; ++i) {
        Offset off = i * TARGET_MACHINE_SIZE;
        insert_inst(LD, {preg_mgr.temp(i), create_imm(off), sp});
    }
    insert_inst(ADDI, {sp, sp, create_imm(stack_grow_size)});

    label->get_insts().erase(inst);
}

Instruction *CodeGen::insert_inst(mir::MIR_INST op,
                                  std::vector<mir::Value *> vec) {
    auto label = _upgrade_context.label;
    auto inst = _upgrade_context.inst;
    return &label->insert_before(inst, op, vec);
}
