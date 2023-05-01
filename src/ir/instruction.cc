#include "instruction.hh"
#include "basic_block.hh"
#include "function.hh"
#include "type.hh"

#include <cassert>
#include <string>
#include <vector>

#define assertm(exp, msg) assert(((void)msg, exp))
using namespace ir;
using std::vector;
using std::string;

Instruction::Instruction(Type *type, const string& name, OpID id, unsigned num_ops, 
                         vector<Value *> &operands, BasicBlock *parent)
    : User(type, name, operands), _id(id), _num_ops(num_ops), _parent(parent) {}

RetInst::RetInst(Type *type, OpID id, vector<Value *> &operands,
                 BasicBlock *parent)
    : Instruction(type, "RetInst", id, 1, operands, parent) {}
Instruction *RetInst::create(OpID id, std::vector<Value *> &&operands,
                             BasicBlock *parent) {
    return new RetInst(parent->get_function()->get_module()->get_void_type(),
                       id, operands, parent);
}
string RetInst::print() const{
    return "return " + this->operands()[0]->get_name();
}

BrInst::BrInst(Type *type, OpID id, vector<Value *> &operands,
               BasicBlock *parent)
    : Instruction(type, "BrInst", id, operands.size(), operands, parent) {}
Instruction *BrInst::create(OpID id, std::vector<Value *> &&operands,
                            BasicBlock *parent) {
    return new BrInst(parent->get_function()->get_module()->get_void_type(), id,
                      operands, parent);
}
string BrInst::print() const{
    if(this->operands().size()==1)
        return "goto " + this->operands()[0]->get_name();
    else
        return "if " + this->operands()[0]->get_name() + " goto" 
            + this->operands()[1]->get_name() + " else goto" + this->operands()[2]->get_name();
}

BinaryInst::BinaryInst(Type *type, OpID id, vector<Value *> &operands,
                       BasicBlock *parent)
    : Instruction(type, "op" + std::to_string(parent->get_function()->get_seq()), id, 2, operands, parent) {}
Instruction *BinaryInst::create(OpID id, std::vector<Value *> &&operands,
                                BasicBlock *parent) {
    if (is_int_bina(id))
        return new BinaryInst(
            parent->get_function()->get_module()->get_int_type(), id, operands,
            parent);
    else if (is_float_bina(id))
        return new BinaryInst(
            parent->get_function()->get_module()->get_float_type(), id,
            operands, parent);
    else
        assertm(false, "You have passed the wrong OpID!");
}
string BinaryInst::print() const{
    string OpName;
    switch (_id) {
        case add:OpName="+";break;
        case sub:OpName="-";break;
        case mul:OpName="*";break;
        case sdiv:OpName="/";break;
        case fadd:OpName="+.";break;// The dot next to the operator sign indicates a floating point operation  
        case fsub:OpName="-.";break;
        case fmul:OpName="*.";break;
        case fdiv:OpName="/.";break;
        default:assertm(false, "The op of BinaryInst is wrong!");break;
    }
    return this->get_name() + " = " + this->operands()[0]->get_name() + OpName + this->operands()[1]->get_name();
}

AllocaInst::AllocaInst(Type *type, OpID id, vector<Value *> &&operands,
                       BasicBlock *parent)
    : Instruction(type, "op" + std::to_string(parent->get_function()->get_seq()), id, 0, operands, parent) {}
Instruction *AllocaInst::create(Type *element_ty, OpID id, BasicBlock *parent) {
    return new AllocaInst(
        parent->get_function()->get_module()->get_pointer_type(element_ty), id,
        {}, parent);
}
string AllocaInst::print() const{
    return this->get_name() + " = Alloca " + static_cast<const PointerType* >(this->get_type())->get_element_type()->print();
}

LoadInst::LoadInst(Type *type, OpID id, vector<Value *> &operands,
                   BasicBlock *parent)
    : Instruction(type, "op" + std::to_string(parent->get_function()->get_seq()), id, 1, operands, parent) {}
Instruction *LoadInst::create(OpID id, std::vector<Value *> &&operands,
                              BasicBlock *parent) {
    Type *element_type = dynamic_cast<const PointerType *>(operands[0]->get_type())
                             ->get_element_type();
    Type *inst_type;
    if (element_type->is_int_type())
        inst_type = parent->get_function()->get_module()->get_int_type();
    else if (element_type->is_float_type())
        inst_type = parent->get_function()->get_module()->get_float_type();
    else if (element_type->is_array_type())
        inst_type = parent->get_function()->get_module()->get_array_type(
            dynamic_cast<ArrayType*>(element_type)->get_element_type(), dynamic_cast<ArrayType*>(element_type)->get_length());
    else assertm(false, "The load target is wrong!");
    return new LoadInst(inst_type, id, operands, parent);
}
string LoadInst::print() const{
    return this->get_name() + " = Load " + this->operands()[0]->get_name();
}

StoreInst::StoreInst(Type *type, OpID id, vector<Value *> &operands,
                     BasicBlock *parent)
    : Instruction(type, "Store", id, 2, operands, parent) {}
Instruction *StoreInst::create(OpID id, std::vector<Value *> &&operands,
                               BasicBlock *parent) {
    return new StoreInst(parent->get_function()->get_module()->get_void_type(),
                         id, operands, parent);
}
string StoreInst::print() const{
    return "Store " + this->operands()[0]->get_name() + " into " + this->operands()[1]->get_name();
}

CmpInst::CmpInst(Type *type, CmpOp id, vector<Value *> &operands,
                 BasicBlock *parent)
    : Instruction(type, "op" + std::to_string(parent->get_function()->get_seq()), OpID::cmp, 2, operands, parent), _id(id) {}
Instruction *CmpInst::create(CmpOp id, std::vector<Value *> &&operands,
                             BasicBlock *parent) {
    return new CmpInst(parent->get_function()->get_module()->get_int_type(), id,
                       operands, parent);
}
string CmpInst::print() const{
    string CmpName;
    switch (_id) {
        case EQ:CmpName="==";break;
        case NE:CmpName="!=";break;
        case GT:CmpName=">";break;
        case GE:CmpName=">=";break;
        case LT:CmpName="<";break;
        case LE:CmpName="<=";break;
    }
    return this->get_name() + " = " + this->operands()[0]->get_name() + CmpName + this->operands()[1]->get_name();
}

FCmpInst::FCmpInst(Type *type, FCmpOp id, vector<Value *> &operands,
                 BasicBlock *parent)
    : Instruction(type, "op" + std::to_string(parent->get_function()->get_seq()), OpID::fcmp, 2, operands, parent), _id(id) {}
Instruction *FCmpInst::create(FCmpOp id, std::vector<Value *> &&operands,
                              BasicBlock *parent) {
    return new FCmpInst(parent->get_function()->get_module()->get_int_type(),
                        id, operands, parent);
}
string FCmpInst::print() const{
    string FCmpName;
    switch (_id) {
        case EQ:FCmpName="==.";break;
        case NE:FCmpName="!=.";break;
        case GT:FCmpName=">.";break;
        case GE:FCmpName=">=.";break;
        case LT:FCmpName="<.";break;
        case LE:FCmpName="<=.";break;
    }
    return this->get_name() + " = " + this->operands()[0]->get_name() + FCmpName + this->operands()[1]->get_name();
}

CallInst::CallInst(Type *type, OpID id, vector<Value *> &operands,
                   BasicBlock *parent)
    : Instruction(type, "op" + std::to_string(parent->get_function()->get_seq()), 
                    id, operands.size(), operands, parent) {}
Instruction *CallInst::create(OpID id, std::vector<Value *> &&operands,
                              BasicBlock *parent) {
    return new CallInst(
        dynamic_cast<const FuncType *>(operands[0]->get_type())->get_result_type(),
        id, operands, parent);
}
string CallInst::print() const{
    string head;
    string args;
    if(this->get_type()->is_void_type())
        head = "Call ";
    else 
        head = this->get_name() + " = Call ";
    for(const auto& oper : this->operands())
        args += " " + oper->get_type()->print() + ":" + oper->get_name() + " ";
    return head + this->get_type()->print() + " " + this->operands()[0]->get_name() + " (" + args + ")";
}

Fp2siInst::Fp2siInst(Type *type, OpID id, vector<Value *> &operands,
                     BasicBlock *parent)
    : Instruction(type, "op" + std::to_string(parent->get_function()->get_seq()), id, 1, operands, parent) {}
Instruction *Fp2siInst::create(OpID id, std::vector<Value *> &&operands,
                               BasicBlock *parent) {
    return new Fp2siInst(parent->get_function()->get_module()->get_int_type(),
                         id, operands, parent);
}
string Fp2siInst::print() const{
    return this->get_name() + " = (int)" + this->operands()[0]->get_name();
}

Si2fpInst::Si2fpInst(Type *type, OpID id, vector<Value *> &operands,
                     BasicBlock *parent)
    : Instruction(type, "op" + std::to_string(parent->get_function()->get_seq()), id, 1, operands, parent) {}
Instruction *Si2fpInst::create(OpID id, std::vector<Value *> &&operands,
                               BasicBlock *parent) {
    return new Si2fpInst(parent->get_function()->get_module()->get_float_type(),
                         id, operands, parent);
}
string Si2fpInst::print() const{
    return this->get_name() + " = (float)" + this->operands()[0]->get_name();
}