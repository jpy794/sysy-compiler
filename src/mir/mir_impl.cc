#include "mir_config.hh"
#include "mir_function.hh"
#include "mir_instruction.hh"

#include <map>
#include <string_view>

using namespace std;
using namespace mir;

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

/* prints */

void Instruction::dump(std::ostream &os, const MIRContext &context) const {
    MIRContext op_context{context.stage, Role::NameOnly};
    switch (context.stage) {
    case Stage::stage1:
        os << "\t" << (_partial ? "*" : "") << MIR_INST_NAME.at(_opcode) << " ";
        for (unsigned i = 0; i < _operands.size(); i++) {
            _operands[i]->dump(os, op_context);
            if (i != _operands.size() - 1)
                os << ", ";
        }
        break;
    case Stage::stage2:
        throw not_implemented_error{};
        break;
    }
}
