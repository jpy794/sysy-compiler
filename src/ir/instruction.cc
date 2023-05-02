#include "instruction.hh"
#include "basic_block.hh"
#include "err.hh"
#include "module.hh"
#include "type.hh"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ir;
using namespace std;

Instruction::Instruction(BasicBlock *bb, Type *type, OpID id,
                         vector<Value *> &&operands)
    : User(bb->module(), type, "%op" + to_string(bb->get_function()->get_seq()),
           std::move(operands)),
      _id(id) {}

RetInst::RetInst(BasicBlock *bb, std::vector<Value *> &&operands)
    : Instruction(bb, bb->module()->get_void_type(), ret, std::move(operands)) {
    if (bb->get_function()->get_return_type()->is_void_type())
        assert(operands.size() == 0);
    else {
        assert(operands.size() == 1);
        assert(operands[0]->get_type() ==
               bb->get_function()->get_return_type());
    }
}

BrInst::BrInst(BasicBlock *bb, std::vector<Value *> &&operands)
    : Instruction(bb, bb->module()->get_void_type(), br, std::move(operands)) {
    assert(operands.size() == 1 or operands.size() == 3);
    if (operands.size() == 1) {
        // basic_block
        assert(operands[0]->get_type()->is_label_type());
    } else if (operands.size() == 3) {
        auto int_type = dynamic_cast<IntType *>(operands[0]->get_type());
        assert(int_type and int_type->get_num_bits() == 1);
        assert(operands[1]->get_type()->is_label_type());
        assert(operands[2]->get_type()->is_label_type());
    } else
        throw unreachable_error{"branch has only 1 or 3 operands"};
}

Type *BinaryInst::_deduce_type(BasicBlock *bb, BinOp op) {
    Type *type{nullptr};
    if (find(_int_op.begin(), _int_op.end(), op) != _int_op.end()) {
        type = bb->module()->get_int32_type();
    } else if (find(_float_op.begin(), _float_op.end(), op) !=
               _float_op.end()) {
        type = bb->module()->get_float_type();
    } else {
        throw unreachable_error{};
    }
    return type;
}

BinaryInst::BinaryInst(BasicBlock *bb, std::vector<Value *> &&operands,
                       BinOp op)
    : Instruction(bb, _deduce_type(bb, op), _op_map[op], std::move(operands)) {
    assert(operands.size() == 2);
    // self type is deduced from opid
    assert(get_type() == operands[0]->get_type());
    assert(get_type() == operands[1]->get_type());
}

AllocaInst::AllocaInst(BasicBlock *bb, std::vector<Value *> &&operands,
                       Type *elem_type)
    : Instruction(bb, bb->module()->get_pointer_type(elem_type), br,
                  std::move(operands)) {
    // alloca is a ptr type itself, and its op(type) can be deduced from that
    assert(operands.size() == 0);
}

Type *LoadInst::_deduce_type(BasicBlock *bb,
                             const std::vector<Value *> &operands) {
    if (operands.size() < 1) {
        throw logic_error{"expect op0 exisits"};
    }
    auto op0_type = dynamic_cast<PointerType *>(operands[0]->get_type());
    if (not op0_type) {
        throw logic_error{"op0 of load is not a pointer"};
    }
    Type *element_type = op0_type->get_element_type();
    Type *inst_type{nullptr};
    if (element_type->is_int_type()) {
        inst_type = bb->module()->get_int32_type();
    } else if (element_type->is_float_type()) {
        inst_type = bb->module()->get_float_type();
    } else if (element_type->is_pointer_type()) {
        auto elem_ptr_type = dynamic_cast<PointerType *>(element_type);
        if (not elem_ptr_type) {
            throw logic_error{"elem has pointer id but is not an pointer type"};
        }
        inst_type = elem_ptr_type;
    } else {
        throw logic_error("The load target is wrong!");
    }
    return inst_type;
}

LoadInst::LoadInst(BasicBlock *bb, std::vector<Value *> &&operands)
    : Instruction(bb, _deduce_type(bb, operands), load, std::move(operands)) {
    assert(operands.size() == 1);
}

StoreInst::StoreInst(BasicBlock *bb, std::vector<Value *> &&operands)
    : Instruction(bb, bb->module()->get_void_type(), store,
                  std::move(operands)) {
    assert(operands.size() == 2);
    auto ptr_type = dynamic_cast<PointerType *>(operands[1]->get_type());
    assert(ptr_type and
           ptr_type->get_element_type() == operands[0]->get_type());
}

Instruction::OpID CmpInst::_deduce_id(CmpOp cmp_op) {
    OpID id;
    if (find(_int_op.begin(), _int_op.end(), cmp_op) != _int_op.end()) {
        id = cmp;
    } else if (find(_float_op.begin(), _float_op.end(), cmp_op) !=
               _float_op.end()) {
        id = fcmp;
    } else {
        throw unreachable_error{};
    }
    return id;
}

CmpInst::CmpInst(BasicBlock *bb, std::vector<Value *> &&operands, CmpOp cmp_op)
    : Instruction(bb, bb->module()->get_int1_type(), _deduce_id(cmp_op),
                  std::move(operands)) {
    assert(operands.size() == 2);
    if (_id == cmp) {
        assert(operands[0]->get_type()->is_int_type());
        assert(operands[1]->get_type()->is_int_type());
        // TODO: check it's i32 type
        // (lxq:leave it)
    } else if (_id == fcmp) {
        assert(operands[0]->get_type()->is_float_type());
        assert(operands[0]->get_type()->is_float_type());
    } else
        throw unreachable_error{};
}

Type *CallInst::_deduce_type(BasicBlock *bb,
                             const std::vector<Value *> &operands) {
    if (operands.size() < 1) {
        throw logic_error{"expect op0 exisits"};
    }
    auto op0_type = operands[0]->get_type();
    if (not op0_type->is_function_type()) {
        throw logic_error{"expect op0 to be function"};
    }
    auto func_type = dynamic_cast<FuncType *>(op0_type);
    if (not func_type) {
        throw logic_error{"is_function_type but not functype"};
    }
    return func_type->get_result_type();
}

CallInst::CallInst(BasicBlock *bb, vector<Value *> &&operands)
    : Instruction(bb, _deduce_type(bb, operands), call, std::move(operands)) {
    assert(operands.size() >= 1);
    // _deduce_type has already checked the type
    auto functype = static_cast<FuncType *>(operands[0]->get_type());
    assert(1 + functype->get_num_params() == operands.size());
    for (int i = 0; i < functype->get_num_params(); ++i)
        assert(functype->get_param_type(i) == operands[i + 1]->get_type());
}

Fp2siInst::Fp2siInst(BasicBlock *bb, std::vector<Value *> &&operands)
    : Instruction(bb, bb->module()->get_int32_type(), fptosi,
                  std::move(operands)) {
    assert(operands.size() == 1);
    assert(operands[0]->get_type()->is_int_type());
}

Si2fpInst::Si2fpInst(BasicBlock *bb, std::vector<Value *> &&operands)
    : Instruction(bb, bb->module()->get_float_type(), sitofp,
                  std::move(operands)) {
    assert(operands.size() == 1);
    assert(operands[0]->get_type()->is_float_type());
}

Type *GetElementPtrInst::_deduce_type(BasicBlock *bb,
                                      const std::vector<Value *> &operands) {
    Type *type = operands[0]->get_type();
    for (unsigned i = 1; i < operands.size(); i++)
        if (dynamic_cast<PointerType *>(type))
            type = dynamic_cast<PointerType *>(type)->get_element_type();
        else if (dynamic_cast<ArrayType *>(type))
            type = dynamic_cast<ArrayType *>(type)->get_element_type();
        else
            throw logic_error{"expected less index of array"};
    return type;
}

GetElementPtrInst::GetElementPtrInst(BasicBlock *bb,
                                     std::vector<Value *> &&operands)
    : Instruction(bb, bb->module()->get_float_type(), getelementptr,
                  std::move(operands)) {}

ZextInst::ZextInst(BasicBlock *bb, std::vector<Value *> &&operands)
    : Instruction(bb, bb->module()->get_int32_type(), zext,
                  std::move(operands)) {}
/* ===== print ir ===== */

string RetInst::print() const {
    if (this->operands().size() != 0) // ret <type> <value>
        return "ret " + this->operands()[0]->get_type()->print() + " " +
               print_op(this->operands()[0]);
    else // ret void
        return "ret void";
}

string BrInst::print() const {
    if (this->operands().size() == 1)
        return "br label " + print_op(this->operands()[0]);
    else
        return "br i1 " + print_op(this->operands()[0]);
    +", label " + print_op(this->operands()[1]) + ", label " +
        print_op(this->operands()[2]);
}

string BinaryInst::print() const {
    string OpName;
    switch (_id) {
    case add:
        OpName = "add";
        break;
    case sub:
        OpName = "sub";
        break;
    case mul:
        OpName = "mul";
        break;
    case sdiv:
        OpName = "sdiv";
        break;
    case srem:
        OpName = "srem";
        break;
    case fadd:
        OpName = "fadd";
        break;
    case fsub:
        OpName = "fsub";
        break;
    case fmul:
        OpName = "fmul";
        break;
    case fdiv:
        OpName = "fdiv";
        break;
    case frem:
        OpName = "frem";
        break;
    default:
        throw logic_error{"The op of BinaryInst is wrong!"};
        break;
    }
    return print_op(this) + " = " + OpName + this->get_type()->print() +
           print_op(this->operands()[0]) + ", " + print_op(this->operands()[1]);
}

string AllocaInst::print() const {
    return print_op(this) + " = alloca " +
           static_cast<const PointerType *>(this->get_type())
               ->get_element_type()
               ->print();
}

string LoadInst::print() const {
    return this->get_name() + " = load " + this->get_type()->print() + ", " +
           print_op(this->operands()[0]);
}

string StoreInst::print() const {
    return "store " + this->operands()[0]->get_type()->print() +
           print_op(this->operands()[0]) + " ," +
           this->operands()[1]->get_type()->print() +
           print_op(this->operands()[1]);
}

string CmpInst::print() const {
    string CmpName;
    string cmp;
    switch (_cmp_op) {
    case EQ:
        CmpName = "eq";
        cmp = "icmp";
        break;
    case NE:
        CmpName = "ne";
        cmp = "icmp";
        break;
    case GT:
        CmpName = "sgt";
        cmp = "icmp";
        break;
    case GE:
        CmpName = "sge";
        cmp = "icmp";
        break;
    case LT:
        CmpName = "slt";
        cmp = "icmp";
        break;
    case LE:
        CmpName = "sle";
        cmp = "icmp";
        break;
    case FEQ:
        CmpName = "oeq";
        cmp = "fcmp";
        break;
    case FNE:
        CmpName = "oge";
        cmp = "fcmp";
        break;
    case FGT:
        CmpName = "ogt";
        cmp = "fcmp";
        break;
    case FGE:
        CmpName = "oge";
        cmp = "fcmp";
        break;
    case FLT:
        CmpName = "olt";
        cmp = "fcmp";
        break;
    case FLE:
        CmpName = "ole";
        cmp = "fcmp";
        break;
    default:
        break;
    }

    return print_op(this) + " = " + cmp + " " + CmpName + " " +
           this->operands()[0]->get_type()->print() + " " +
           print_op(this->operands()[0]) + ", " + print_op(this->operands()[1]);
}

string CallInst::print() const {
    string head;
    string args;
    if (this->get_type()->is_void_type())
        head = "call ";
    else
        head = this->get_name() + " = call ";
    for (const auto &oper : this->operands())
        args += oper->get_type()->print() + " " + print_op(oper) + ", ";
    args.erase(args.length() - 2, 2);
    return head + this->get_type()->print() + " " +
           print_op(this->operands()[0]) + " (" + args + ")";
}

string Fp2siInst::print() const {
    return print_op(this) + " = fptosi float " + print_op(this->operands()[0]) +
           " to " + this->operands()[1]->print();
}

string Si2fpInst::print() const {
    return print_op(this) + " = sitofp " + this->get_type()->print() + " " +
           print_op(this->operands()[0]) + " to float";
}

string GetElementPtrInst::print() const {
    string index;
    for (const auto &op : this->operands())
        index += ", " + op->get_type()->print() + " " + print_op(op);
    return print_op(this) + " = getelementptr " +
           static_cast<const PointerType *>(this->operands()[0]->get_type())
               ->get_element_type()
               ->print() +
           index;
}

string ZextInst::print() const {
    return print_op(this) + " = zext i1" + print_op(this->operands()[0]) +
           " to i32";
}
