#include "context.hh"
#include "mir_memory.hh"
#include "mir_module.hh"

#include <map>
#include <string_view>

using namespace std;
using namespace mir;
using namespace context;

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
    {JAL, "jal"},
    {JALR, "jalr"},
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

    switch (context.stage) {
    case Stage::stage1: {
        switch (context.role) {
        case Role::Full: {
            os << _name << ":\n";

            auto &cfg_info = context.allocator.get_cfg_info(this);
            os << "# =========DFS ORDER=========\n";
            os << "# ";
            for (auto label : cfg_info.label_order) {
                label->dump(os, name_only_context);
                os << " ";
            }
            os << "\n";

            for (auto label : _labels)
                label->dump(os, func_context);
        } break;
        case Role::NameOnly:
            os << _name;
            break;
        }
    } break;
    case Stage::stage2:
        // add stack alloc and related stack offset map
        throw not_implemented_error{};
        break;
    }
}

void Label::dump(std::ostream &os, const Context &context) const {
    switch (context.role) {
    case Role::Full: {
        os << _name << ":\n";
        for (auto &inst : _insts) {
            os << "\t";
            inst.dump(os, context);
            os << "\n";
        }
    } break;
    case Role::NameOnly:
        os << _name;
        break;
    }
}

void Instruction::dump(std::ostream &os, const Context &context) const {
    Context name_only_context{context.name_only()};
    auto inst_id =
        context.allocator.get_cfg_info(context.cur_function).instid.at(this);

    switch (context.stage) {
    case Stage::stage1:
        os << "\t" << inst_id << ". " << (_partial ? "*" : "")
           << MIR_INST_NAME.at(_opcode) << " ";
        for (unsigned i = 0; i < _operands.size(); i++) {
            _operands[i]->dump(os, name_only_context);
            if (i != _operands.size() - 1)
                os << ", ";
        }
        break;
    case Stage::stage2:
        throw not_implemented_error{};
        break;
    }
}

void StatckObject::dump(std::ostream &os, const Context &context) const {
    switch (context.stage) {
    case Stage::stage1: {
        os << "@stack-object";
        break;
    }
    case Stage::stage2:
        throw not_implemented_error{};
    }
}

void GlobalObject::dump(std::ostream &os, const Context &context) const {
    switch (context.role) {
    case Role::Full: {
        os << _name << ":\n";
        size_t off = 0; // byte offset
        for (auto [idx, value] : _inits) {
            if (off < idx * BASIC_TYPE_SIZE) {
                os << "\t.zero " << std::to_string(idx * BASIC_TYPE_SIZE - off)
                   << "\n";
            }
            switch (_type) {
            case BasicType::VOID:
                throw unreachable_error{};
            case BasicType::INT:
                os << "\t.word " << std::get<int>(value) << "\n";
                break;
            case BasicType::FLOAT: {
                float v = std::get<float>(value);
                os << "\t.word 0x" << std::hex << std::uppercase
                   << *reinterpret_cast<uint32_t *>(&v) << " # float " << v
                   << "\n";
                break;
            }
            }
            off = (idx + 1) * BASIC_TYPE_SIZE;
        }
        assert(off <= _size);
        if (off != _size)
            os << "\t.zero " << std::to_string(_size - off) << "\n";
    } break;
    case Role::NameOnly:
        os << _name;
        break;
    }
}
