#include "context.hh"
#include "err.hh"
#include "mir_memory.hh"
#include "mir_module.hh"
#include "mir_register.hh"
#include "regalloc.hh"

#include <map>
#include <ostream>
#include <string_view>

using namespace std;
using namespace mir;
using namespace context;

auto TAB = Context::TAB;
void resolve_indent(std::ostream &os, const Context &context) {
    for (unsigned i = 0; i < context.indent_level; ++i)
        os << TAB;
}

void mir::flatten_array(ir::ConstArray *const_arr, InitPairs &inits,
                        const size_t start) {
    size_t offset = start;
    for (auto const_v : const_arr->array()) {
        if (is_a<ir::ConstArray>(const_v)) {
            auto sub_arr = as_a<ir::ConstArray>(const_v);
            flatten_array(sub_arr, inits, offset);
            offset += sub_arr->get_type()->as<ir::ArrayType>()->get_total_cnt();
        } else if (is_a<ir::ConstZero>(const_v)) {
            auto type = const_v->get_type();
            if (type->is_basic_type())
                offset += 1;
            else
                offset += type->as<ir::ArrayType>()->get_total_cnt();
        } else if (is_a<ir::ConstInt>(const_v)) {
            auto v = as_a<ir::ConstInt>(const_v)->val();
            if (v)
                inits.push_back({offset, v});
            offset++;
        } else if (is_a<ir::ConstFloat>(const_v)) {
            auto v = as_a<ir::ConstFloat>(const_v)->val();
            if (v)
                inits.push_back({offset, v});
            offset++;
        } else
            throw unreachable_error{};
    }
}

const map<MIR_INST, string_view> MIR_INST_NAME = {
    // start
    // {LUI, "lui"},
    // {AUIPC, "auipc"},
    /* {JAL, "jal"},
     * {JALR, "jalr"}, */
    {BEQ, "beq"},
    {BNE, "bne"},
    {BLT, "blt"},
    {BGE, "bge"},
    // {LB, "lb"},
    // {LH, "lh"},
    {LW, "lw"},
    // {SB, "sb"},
    // {SH, "sh"},
    {SW, "sw"},
    {SLTI, "slti"},
    {XORI, "xori"},
    {ORI, "ori"},
    {ANDI, "andi"},
    {ADD, "add"},
    {SUB, "sub"},
    {SLT, "slt"},
    {XOR, "xor"},
    {OR, "or"},
    {AND, "and"},
    {LD, "ld"},
    {SD, "sd"},
    {ADDI, "addi"},
    {ADDIW, "addiw"},
    {SLLIW, "slliw"},
    {SRLIW, "srliw"},
    {SRAIW, "sraiw"},
    {ADDW, "addw"},
    {SUBW, "subw"},
    {SLLW, "sllw"},
    {SRLW, "srlw"},
    {SRAW, "sraw"},
    {MUL, "mul"},
    {MULW, "mulw"},
    {DIVW, "divw"},
    {REMW, "remw"},
    {FLW, "flw"},
    {FSW, "fsw"},
    {FADDS, "fadd.s"},
    {FSUBS, "fsub.s"},
    {FMULS, "fmul.s"},
    {FDIVS, "fdiv.s"},
    {FCVTSW, "fcvt.s.w"},
    {FCVTWS, "fcvt.w.s"},
    {FMVXW, "fmv.x.w"},
    {FMVWX, "fmv.w.x"},
    {FLTS, "flt.s"},
    {FLES, "fle.s"},
    {FEQS, "feq.s"},
    // pseudo instruction
    {Move, "mv"},
    {Jump, "j"},
    {LoadAddress, "la"},
    {LoadImmediate, "li"},
    {SetEQZ, "seqz"},
    {SetNEQZ, "snez"},
    {Call, "call"},
    {Ret, "ret"}
    // end
};

bool Instruction::will_write_register() const {
    if (_opcode >= MIR_INST::Jump)
        return false;
    if (_opcode == MIR_INST::Call)
        return not is_a<Function>(_operands[0]);
    return true;
}

/* implemention of dumps */

ostream &operator<<(ostream &os, const codegen::LiveInterVal &interval) {
    os << "{" << interval.vreg_id << ",<" << interval.start << ","
       << interval.end << ">}";
    return os;
}

void Module::dump(std::ostream &os, const Context &context) const {
    for (auto func : _functions) {
        if (not func->is_definition())
            continue;
        func->dump(os, context);
    }
    // output global at the end
    for (auto global : _globals) {
        global->dump(os, context);
    }
}

void Function::dump(std::ostream &os, const Context &context) const {
    auto func_context{context};
    func_context.cur_function = this;
    auto name_only_context{func_context.name_only()};

    if (context.role == Role::NameOnly) {
        os << _name;
        return;
    }

    switch (context.stage) {
    case Stage::stage1: {
        os << _name << ":\n";
        auto &allocator = context.allocator;

        auto &cfg_info = allocator.get_cfg_info(this);
        os << "# =========DFS ORDER=========\n";
        os << "# ";
        for (auto label : cfg_info.label_order) {
            label->dump(os, name_only_context);
            os << " ";
        }
        os << "\n";

        auto &intervals = allocator.get_live_ints(this, false);
        os << "# =========Live Interval=========\n";
        os << "# ";
        for (auto &interval : intervals) {
            os << interval << " ";
        }
        os << "\n";

        auto &regmap = allocator.get_reg_map(this, false);
        os << "# =========Register Map=========\n";
        os << "# ";
        for (auto &[vid, reginfo] : regmap) {
            os << vid << "~";
            reginfo.reg->dump(os, name_only_context);
            os << " ";
        }
        os << "\n";

        for (auto label : _labels)
            label->dump(os, func_context);
    } break;
    case Stage::stage2:
        // add stack alloc and related stack offset map
        os << _name << ":\n";
        for (auto label : _labels)
            label->dump(os, func_context);
        break;
    }
}

void Label::dump(std::ostream &os, const Context &context) const {
    auto indent_context{context.indent()};
    switch (context.role) {
    case Role::Full: {
        if (this != context.cur_function->get_labels()[0])
            os << _name << ":\n";
        for (auto &inst : _insts) {
            inst.dump(os, indent_context);
        }
    } break;
    case Role::NameOnly:
        os << _name;
        break;
    }
}

void Instruction::dump(std::ostream &os, const Context &context) const {
    Context name_only_context{context.name_only()};

    auto cur_func = context.cur_function;

    auto indent = [&]() { resolve_indent(os, context); };

    switch (context.stage) {
    case Stage::stage1: {
        auto inst_id = context.allocator.get_cfg_info(cur_func).instid.at(this);
        auto &live_var_set = context.allocator.get_liveness(cur_func, false);
        // live_var in set
        indent();
        os << "# in-set: ";
        for (auto live_var_id : live_var_set.at(codegen::IN_POINT(inst_id)))
            os << live_var_id << " ";
        os << "\n";
        // instruction content
        indent();
        os << inst_id << ". " << (_partial ? "*" : "")
           << MIR_INST_NAME.at(_opcode) << " ";
        for (unsigned i = 0; i < _operands.size(); i++) {
            _operands[i]->dump(os, name_only_context);
            if (i != _operands.size() - 1)
                os << ", ";
        }
        os << "\n";
        // live_var out set
        indent();
        os << "# out-set: ";
        for (auto live_var_id : live_var_set.at(codegen::OUT_POINT(inst_id)))
            os << live_var_id << " ";
        os << "\n";
    } break;
    case Stage::stage2:
        // instruction content
        // TODO special case for instructions with offset
        indent();
        os << MIR_INST_NAME.at(_opcode) << " ";
        if (is_load_store()) {
            assert(get_operand_num() == 3);
            _operands[0]->dump(os, name_only_context);
            os << ", ";
            _operands[1]->dump(os, name_only_context);
            os << "(";
            _operands[2]->dump(os, name_only_context);
            os << ")";

        } else {
            for (unsigned i = 0; i < _operands.size(); i++) {
                _operands[i]->dump(os, name_only_context);
                if (i != _operands.size() - 1)
                    os << ", ";
            }
        }
        os << "\n";
        break;
    }
}

void StatckObject::dump(std::ostream &os, const Context &context) const {
    switch (context.stage) {
    case Stage::stage1:
        os << "@stack-object[" << _size << "," << _align << "]";
        break;
    case Stage::stage2:
        os << "@stack-object";
        break;
    }
}

void GlobalObject::dump(std::ostream &os, const Context &context) const {
    switch (context.role) {
    case Role::Full: {
        os << _name << ":\n";
        size_t off = 0; // byte offset
        for (auto [idx, value] : _inits) {
            if (off < idx * BASIC_TYPE_SIZE) {
                os << TAB << ".zero "
                   << std::to_string(idx * BASIC_TYPE_SIZE - off) << "\n";
            }
            switch (_type) {
            case BasicType::VOID:
                throw unreachable_error{};
            case BasicType::INT:
                os << TAB << ".word " << std::get<int>(value) << "\n";
                break;
            case BasicType::FLOAT: {
                float v = std::get<float>(value);
                os << TAB << ".word 0x" << std::hex << std::uppercase
                   << *reinterpret_cast<uint32_t *>(&v) << " # float " << v
                   << "\n";
                break;
            }
            }
            off = (idx + 1) * BASIC_TYPE_SIZE;
        }
        assert(off <= _size);
        if (off != _size)
            os << TAB << ".zero " << std::to_string(_size - off) << "\n";
    } break;
    case Role::NameOnly:
        os << _name;
        break;
    }
}

void CalleeSave::dump(std::ostream &os, const Context &context) const {
    auto name_only_context = context.name_only();
    resolve_indent(os, context);
    switch (context.stage) {
    case Stage::stage1:
        throw unreachable_error{};
        break;
    case Stage::stage2:
        os << "@DEBUG-ONLY: callee-save, ";
        if (is_float_reg())
            throw not_implemented_error{};
        else
            PhysicalRegisterManager::get().get_int_reg(_regid)->dump(
                os, name_only_context);
        os << "\n";
        break;
    }
}