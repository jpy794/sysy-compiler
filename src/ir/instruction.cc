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
    assert(not prt->is_terminated());
    assert(ret_val->get_type() == prt->get_func()->get_return_type());
}

RetInst::RetInst(BasicBlock *prt)
    : Instruction(prt, Types::get().void_type(), {}) {
    assert(not prt->is_terminated());
}

BrInst::BrInst(BasicBlock *prt, BasicBlock *to)
    : Instruction(prt, Types::get().void_type(), {to}) {
    assert(not prt->is_terminated());
    link();
}

BrInst::BrInst(BasicBlock *prt, Value *cond, BasicBlock *TBB, BasicBlock *FBB)
    : Instruction(prt, Types::get().void_type(), {cond, TBB, FBB}) {
    assert(is_a<BoolType>(cond->get_type()));
    assert(not prt->is_terminated());
    link();
}

BrInst::~BrInst() { unlink(); }

void BrInst::link() {
    for (auto boba : operands()) {
        if (is_a<BasicBlock>(boba)) {
            BasicBlock::link(_parent, as_a<BasicBlock>(boba));
        }
    }
}

void BrInst::unlink() {
    for (auto boba : operands()) {
        if (is_a<BasicBlock>(boba)) {
            BasicBlock::unlink(_parent, as_a<BasicBlock>(boba));
        }
    }
}
void BrInst::set_operand(size_t idx, Value *value, bool modify_op_use) {
    if (is_a<BasicBlock>(value)) {
        BasicBlock *cur_bb = _parent;
        BasicBlock *old_dest = as_a<BasicBlock>(operands().at(idx));
        BasicBlock *new_dest = as_a<BasicBlock>(value);

        BasicBlock::unlink(cur_bb, old_dest);
        BasicBlock::link(cur_bb, new_dest);
    }
    User::set_operand(idx, value, modify_op_use);
}

IBinaryInst::IBinaryInst(BasicBlock *prt, IBinOp op, Value *lhs, Value *rhs)
    : Instruction(prt, lhs->get_type(), {lhs, rhs}), _op(op) {
    if (op == XOR) {
        assert(is_a<BoolType>(lhs->get_type()));
        assert(is_a<BoolType>(rhs->get_type()));
    } else {
        assert(lhs->get_type() == rhs->get_type());
        assert(get_type()->is<IntType>() or get_type()->is<I64IntType>());
    }
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

ICmpInst::ICmpOp ICmpInst::opposite_icmp_op(ICmpInst::ICmpOp op) {
    switch (op) {
    case EQ:
        return NE;
    case NE:
        return EQ;
    case GT:
        return LT;
    case GE:
        return LE;
    case LT:
        return GT;
    case LE:
        return GE;
    default:
        throw unreachable_error{};
    }
}

ICmpInst::ICmpOp ICmpInst::not_icmp_op(ICmpInst::ICmpOp op) {
    switch (op) {
    case EQ:
        return NE;
    case NE:
        return EQ;
    case GT:
        return LE;
    case GE:
        return LT;
    case LT:
        return GE;
    case LE:
        return GT;
    default:
        throw unreachable_error{};
    }
}

FCmpInst::FCmpInst(BasicBlock *prt, FCmpOp cmp_op, Value *lhs, Value *rhs)
    : Instruction(prt, Types::get().bool_type(), {lhs, rhs}), _cmp_op(cmp_op) {
    assert(is_a<FloatType>(lhs->get_type()));
    assert(is_a<FloatType>(rhs->get_type()));
}

void PhiInst::add_phi_param(Value *val, BasicBlock *bb) {
    assert(this->get_type() == val->get_type());
    this->add_operand(val);
    this->add_operand(bb);
}

PhiInst::PhiInst(BasicBlock *prt, Value *base)
    : Instruction(prt, base->get_type()->as<PointerType>()->get_elem_type(),
                  {}) {}

PhiInst::PhiInst(BasicBlock *prt, Type *type) : Instruction(prt, type, {}) {}

std::vector<PhiInst::Pair> PhiInst::to_pairs() const {
    std::vector<Pair> ret;
    for (auto it = operands().begin(); it != operands().end(); it += 2) {
        auto op = *it;
        auto bb = *(it + 1);
        ret.emplace_back(op, bb->as<BasicBlock>());
    }
    return ret;
}

void PhiInst::from_pairs(const std::vector<Pair> &pairs) {
    release_all_use();
    for (auto [op, bb] : pairs) {
        assert(op->get_type() == get_type());
        add_operand(op);
        add_operand(bb);
    }
}

void PhiInst::rm_phi_param_from(BasicBlock *bb, bool tolerate) {
    for (unsigned i = 1; i != operands().size(); i += 2) {
        if (bb == get_operand(i)) {
            remove_operand(i - 1); // value
            remove_operand(i - 1); // bb
            if (operands().size() == 2)
                replace_all_use_with(get_operand(0));
            else if (operands().size() == 0)
                assert(get_use_list().size() == 0);
            return;
        }
    }
    if (not tolerate)
        throw unreachable_error{};
}

CallInst::CallInst(BasicBlock *prt, Function *func, vector<Value *> &&params)
    : Instruction(prt, func->get_return_type(), _mix2vec(func, params)) {
    auto func_ty = as_a<FuncType>(func->get_type());
    assert(params.size() == func_ty->get_param_types().size());
}

void CallInst::decay_to_void_type() {
    assert(as_a<FuncType>(get_operand(0)->get_type())
               ->get_result_type()
               ->is<VoidType>());
    Value::change_type(Types::get().void_type());
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

TruncInst::TruncInst(BasicBlock *prt, Value *i64)
    : Instruction(prt, Types::get().int_type(), {i64}) {
    assert(is_a<I64IntType>(i64->get_type()));
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
    case XOR:
        OpName = "xor";
        break;
    case LSHR:
        OpName = "lshr";
        break;
    case ASHR:
        OpName = "ashr";
        break;
    case SHL:
        OpName = "shl";
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
        CmpName = "one";
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
    string phi = this->get_name() + " = phi " + this->get_type()->print();
    for (unsigned i = 0; i < operands().size(); i += 2) {

        phi += " [ ";
        phi += operands()[i] ? operands()[i]->get_name() : "nullptr";
        phi += ", %";
        phi += operands()[i + 1] ? operands()[i + 1]->get_name() : "nullptr";
        phi += " ],";
        // phi += " [ " + operands()[i]->get_name() + ", %" +
        // operands()[i + 1]->get_name() + " ],";
    }
    phi.erase(phi.length() - 1, 1);
    return phi;
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
    }
    if (!args.empty())
        args.erase(args.length() - 2, 2);
    return head + " (" + args + ")";
}

string Fp2siInst::print() const {
    return get_name() + " = fptosi float " + operands()[0]->get_name() +
           " to i32";
}

string Si2fpInst::print() const {
    return get_name() + " = sitofp " +
           this->get_operand(0)->get_type()->print() + " " +
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
    return get_name() + " = zext i1 " + operands()[0]->get_name() + " to i32";
}

SextInst::SextInst(BasicBlock *prt, Value *i32)
    : Instruction(prt, Types::get().i64_int_type(), {i32}) {
    assert(is_a<IntType>(i32->get_type()));
}

string SextInst::print() const {
    return get_name() + " = sext i32 " + operands()[0]->get_name() + " to i64";
}

Ptr2IntInst::Ptr2IntInst(BasicBlock *prt, Value *ptr)
    : Instruction(prt, Types::get().i64_int_type(), {ptr}) {
    assert(ptr->get_type()->is<PointerType>());
}

string Ptr2IntInst::print() const {
    return get_name() + " = ptrtoint " + operands()[0]->get_type()->print() +
           " " + operands()[0]->get_name() + " to i64";
}

Int2PtrInst::Int2PtrInst(BasicBlock *prt, Value *val, Type *elem_type)
    : Instruction(prt, Types::get().ptr_type(elem_type), {val}) {
    assert(val->get_type()->is<I64IntType>());
}

string Int2PtrInst::print() const {
    return get_name() + " = inttoptr i64 " + operands()[0]->get_name() +
           " to " + get_type()->print();
}

string TruncInst::print() const {
    return get_name() + " = trunc i64 " + operands()[0]->get_name() + " to i32";
}