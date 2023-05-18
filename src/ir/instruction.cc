#include "instruction.hh"
#include "basic_block.hh"
#include "err.hh"
#include "function.hh"
#include "module.hh"
#include "type.hh"
#include "utils.hh"
#include "value.hh"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace ir;
using namespace std;

Instruction::Instruction(BasicBlock *prt, Type *type,
                         vector<Value *> &&operands)
    : User(type, "%op" + to_string(prt->get_func()->get_inst_seq()),
           std::move(operands)),
      _parent(prt) {}

RetInst::RetInst(BasicBlock *prt, Value *ret_val)
    : Instruction(prt, Types::get().void_type(), {ret_val}) {
    assert(ret_val->get_type() == prt->get_func()->get_return_type());
}

BrInst::BrInst(BasicBlock *prt, BasicBlock *to)
    : Instruction(prt, Types::get().void_type(), {to}) {}

BrInst::BrInst(BasicBlock *prt, Value *cond, BasicBlock *TBB, BasicBlock *FBB)
    : Instruction(prt, Types::get().void_type(), {cond, TBB, FBB}) {
    assert(is_a<BoolType>(cond->get_type()));
}

IBinaryInst::IBinaryInst(BasicBlock *prt, IBinOp op, Value *lhs, Value *rhs)
    : Instruction(prt, Types::get().int_type(), {lhs, rhs}), _op(op) {
    assert(is_a<IntType>(lhs->get_type()));
    assert(is_a<IntType>(rhs->get_type()));
}

FBinaryInst::FBinaryInst(BasicBlock *prt, FBinOp op, Value *lhs, Value *rhs)
    : Instruction(prt, Types::get().float_type(), {lhs, rhs}), _op(op) {
    assert(is_a<FloatType>(lhs->get_type()));
    assert(is_a<FloatType>(rhs->get_type()));
}

AllocaInst::AllocaInst(BasicBlock *prt, Type *elem_type)
    : Instruction(prt, Types::get().ptr_type(elem_type), {}) {
    assert(elem_type->is_basic_type() or is_a<ArrayType>(elem_type));
}

Type *LoadInst::_deduce_type(Value *ptr) {
    Type *elem_type = as_a<PointerType>(ptr->get_type())->get_elem_type();
    // for load target, we only allow basic type and ptr type
    if (elem_type->is_basic_type() or is_a<PointerType>(elem_type))
        return elem_type;
    throw logic_error("The load target is wrong!");
}

LoadInst::LoadInst(BasicBlock *prt, Value *ptr)
    : Instruction(prt, _deduce_type(ptr), {ptr}) {}

StoreInst::StoreInst(BasicBlock *prt, Value *v, Value *ptr)
    : Instruction(prt, Types::get().void_type(), {v, ptr}) {
    assert(v->get_type() ==
           as_a<PointerType>(ptr->get_type())->get_elem_type());
}

ICmpInst::ICmpInst(BasicBlock *prt, ICmpOp cmp_op, Value *lhs, Value *rhs)
    : Instruction(prt, Types::get().bool_type(), {lhs, rhs}), _cmp_op(cmp_op) {
    assert(is_a<IntType>(lhs->get_type()));
    assert(is_a<IntType>(rhs->get_type()));
}

FCmpInst::FCmpInst(BasicBlock *prt, FCmpOp cmp_op, Value *lhs, Value *rhs)
    : Instruction(prt, Types::get().bool_type(), {lhs, rhs}), _cmp_op(cmp_op) {
    assert(is_a<FloatType>(lhs->get_type()));
    assert(is_a<FloatType>(rhs->get_type()));
}

vector<Value *> PhiInst::_get_op(const vector<Value *> &values) {
    vector<Value *> ret;
    ret.reserve(values.size() * 2);
    for (auto v : values) {
        // push back value
        ret.push_back(v);
        // push back bb
        if (is_a<Instruction>(v)) {
            ret.push_back(as_a<Instruction>(v)->get_parent());
        } else if (is_a<Argument>(v)) {
            // FIXME
            // for Argument type, we use entry_bb. Is this right?
            auto &entry_bb = as_a<Argument>(v)->get_function()->get_entry_bb();
            ret.push_back(&entry_bb);

        } else {
            throw unreachable_error{};
        }
    }
    return ret;
}

PhiInst::PhiInst(BasicBlock *prt, std::vector<Value *> &&values)
    : Instruction(prt, values[0]->get_type(), _get_op(values)) {
    // all value should have the same type
    for (auto v : values)
        assert(get_type() == v->get_type());
}

CallInst::CallInst(BasicBlock *prt, Function *func, vector<Value *> &&params)
    : Instruction(prt, func->get_return_type(), _mix2vec(func, params)) {
    auto func_ty = as_a<FuncType>(func->get_type());
    assert(params.size() == func_ty->get_param_types().size());
    for (unsigned i = 0; i < params.size(); ++i)
        assert(params[i]->get_type() == func_ty->get_param_type(i));
}

Fp2siInst::Fp2siInst(BasicBlock *prt, Value *floatv)
    : Instruction(prt, Types::get().int_type(), {floatv}) {
    assert(is_a<FloatType>(floatv->get_type()));
}

Si2fpInst::Si2fpInst(BasicBlock *prt, Value *intv)
    : Instruction(prt, Types::get().float_type(), {intv}) {
    assert(is_a<IntType>(intv->get_type()));
}

Type *GetElementPtrInst::_deduce_type(BasicBlock *bb, Value *baseptr,
                                      const vector<Value *> &offs) {
    Type *type = baseptr->get_type();
    for (unsigned i = 0; i < offs.size(); i++)
        if (is_a<PointerType>(type))
            type = as_a<PointerType>(type)->get_elem_type();
        else if (is_a<ArrayType>(type))
            type = as_a<ArrayType>(type)->get_elem_type();
        else
            throw logic_error{"expected less index of array"};
    return Types::get().ptr_type(type);
}

GetElementPtrInst::GetElementPtrInst(BasicBlock *prt, Value *baseptr,
                                     std::vector<Value *> &&offs)
    : Instruction(prt, _deduce_type(prt, baseptr, offs),
                  _mix2vec(baseptr, offs)) {
    auto elem_type = (as_a<PointerType>(baseptr->get_type())->get_elem_type());
    // FIXME
    // Can baseptr point to other type? Do we have more stricter check?
    assert(is_a<ArrayType>(elem_type) or elem_type->is_basic_type());
    /* 1. the offs' length has been checked through _deduce_type()
     * 2. the multi dim array must contains basic type, we have checked that
     * during ArrayType's construction.
     * */
}

ZextInst::ZextInst(BasicBlock *prt, Value *boolv)
    : Instruction(prt, Types::get().int_type(), {boolv}) {
    assert(is_a<BoolType>(boolv->get_type()));
}

/* ===== print ir ===== */

string RetInst::print() const {
    if (this->operands().size() != 0) // ret <type> <value>
        return "ret " + this->operands()[0]->get_type()->print() + " " +
               this->operands()[0]->get_name();
    else // ret void
        return "ret void";
}

string BrInst::print() const {
    if (this->operands().size() == 1)
        return "br label %" + this->operands()[0]->get_name();
    else
        return "br i1 " + this->operands()[0]->get_name() + ", label %" +
               this->operands()[1]->get_name() + ", label %" +
               this->operands()[2]->get_name();
}

string IBinaryInst::print() const {
    string OpName;
    switch (_op) {
    case ADD:
        OpName = "add";
        break;
    case SUB:
        OpName = "sub";
        break;
    case MUL:
        OpName = "mul";
        break;
    case SDIV:
        OpName = "sdiv";
        break;
    case SREM:
        OpName = "srem";
        break;
    default:
        throw unreachable_error{};
    }
    return get_name() + " = " + OpName + " " + get_type()->print() + " " +
           operands()[0]->get_name() + ", " + operands()[1]->get_name();
}

string FBinaryInst::print() const {
    string OpName;
    switch (_op) {
    case FADD:
        OpName = "fadd";
        break;
    case FSUB:
        OpName = "fsub";
        break;
    case FMUL:
        OpName = "fmul";
        break;
    case FDIV:
        OpName = "fdiv";
        break;
    case FREM:
        OpName = "frem";
        break;
    default:
        throw unreachable_error{};
    }
    return get_name() + " = " + OpName + " " + get_type()->print() + " " +
           operands()[0]->get_name() + ", " + operands()[1]->get_name();
}

string AllocaInst::print() const {
    return get_name() + " = alloca " +
           get_type()->as<PointerType>()->get_elem_type()->print();
}

string LoadInst::print() const {
    return get_name() + " = load " + get_type()->print() + ", " +
           operands()[0]->get_type()->print() + " " + operands()[0]->get_name();
}

string StoreInst::print() const {
    return "store " + operands()[0]->get_type()->print() + " " +
           operands()[0]->get_name() + ", " +
           operands()[1]->get_type()->print() + " " + operands()[1]->get_name();
}

string ICmpInst::print() const {
    string CmpName;
    switch (_cmp_op) {
    case EQ:
        CmpName = "eq";
        break;
    case NE:
        CmpName = "ne";
        break;
    case GT:
        CmpName = "sgt";
        break;
    case GE:
        CmpName = "sge";
        break;
    case LT:
        CmpName = "slt";
        break;
    case LE:
        CmpName = "sle";
        break;
    default:
        throw unreachable_error{};
    }

    return this->get_name() + " = icmp " + CmpName + " " +
           this->operands()[0]->get_type()->print() + " " +
           this->operands()[0]->get_name() + ", " +
           this->operands()[1]->get_name();
}

string FCmpInst::print() const {
    string CmpName;
    switch (_cmp_op) {
    case FEQ:
        CmpName = "oeq";
        break;
    case FNE:
        CmpName = "oge";
        break;
    case FGT:
        CmpName = "ogt";
        break;
    case FGE:
        CmpName = "oge";
        break;
    case FLT:
        CmpName = "olt";
        break;
    case FLE:
        CmpName = "ole";
        break;
    default:
        break;
    }

    return this->get_name() + " = fcmp " + CmpName + " " +
           this->operands()[0]->get_type()->print() + " " +
           this->operands()[0]->get_name() + ", " +
           this->operands()[1]->get_name();
}

string PhiInst::print() const {
    return this->get_name() + " = phi " + this->get_type()->print() + "[" +
           this->operands()[0]->get_name() + ", " +
           this->operands()[1]->get_name() + "]" + "[" +
           this->operands()[3]->get_name() + ", " +
           this->operands()[4]->get_name() + "]";
}

string CallInst::print() const {
    string head;
    string args;
    if (get_type()->is<VoidType>())
        head = "call ";
    else
        head = this->get_name() + " = call ";
    head +=
        operands()[0]->get_type()->as<FuncType>()->get_result_type()->print() +
        " " + operands()[0]->get_name();
    for (unsigned i = 1; i < operands().size(); i++) {
        args += operands()[i]->get_type()->print() + " " +
                operands()[i]->get_name() + ", ";
        args.erase(args.length() - 2, 2);
    }
    return head + " (" + args + ")";
}

string Fp2siInst::print() const {
    return get_name() + " = fptosi float " + operands()[0]->get_name() +
           " to i32";
}

string Si2fpInst::print() const {
    return get_name() + " = sitofp " + get_type()->print() + " " +
           operands()[0]->get_name() + " to float";
}

string GetElementPtrInst::print() const {
    string index;
    for (const auto &op : operands())
        index += ", " + op->get_type()->print() + " " + op->get_name();
    return get_name() + " = getelementptr " +
           operands()[0]
               ->get_type()
               ->as<PointerType>()
               ->get_elem_type()
               ->print() +
           index;
}

string ZextInst::print() const {
    return get_name() + " = zext i1" + operands()[0]->get_name() + " to i32";
}
