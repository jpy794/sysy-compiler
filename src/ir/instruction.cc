#include "instruction.hh"
#include "basic_block.hh"
#include "function.hh"
#include "type.hh"

#include <vector>

using namespace ir;
using std::vector;

Instruction::Instruction(Type *type, OpID id, unsigned num_ops,
                         vector<Value *> &operands, BasicBlock *parent)
    : User(type, operands), _id(id), _num_ops(num_ops), _parent(parent) {}

RetInst::RetInst(Type *type, OpID id, vector<Value *> &operands,
                 BasicBlock *parent)
    : Instruction(type, id, 1, operands, parent) {}
Instruction *RetInst::create(OpID id, std::vector<Value *> &&operands,
                             BasicBlock *parent) {
    return new RetInst(parent->get_function()->get_module()->get_void_type(),
                       id, operands, parent);
}

BrInst::BrInst(Type *type, OpID id, vector<Value *> &operands,
               BasicBlock *parent)
    : Instruction(type, id, operands.size(), operands, parent) {}
Instruction *BrInst::create(OpID id, std::vector<Value *> &&operands,
                            BasicBlock *parent) {
    return new BrInst(parent->get_function()->get_module()->get_void_type(), id,
                      operands, parent);
}

BinaryInst::BinaryInst(Type *type, OpID id, vector<Value *> &operands,
                       BasicBlock *parent)
    : Instruction(type, id, 2, operands, parent) {}
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
        assert(false && "You have passed the wrong OpID!");
}

AllocaInst::AllocaInst(Type *type, OpID id, vector<Value *> &&operands,
                       BasicBlock *parent)
    : Instruction(type, id, 0, operands, parent) {}
Instruction *AllocaInst::create(Type *element_ty, OpID id, BasicBlock *parent) {
    return new AllocaInst(
        parent->get_function()->get_module()->get_pointer_type(element_ty), id,
        {}, parent);
}

LoadInst::LoadInst(Type *type, OpID id, vector<Value *> &operands,
                   BasicBlock *parent)
    : Instruction(type, id, 1, operands, parent) {}
Instruction *LoadInst::create(OpID id, std::vector<Value *> &&operands,
                              BasicBlock *parent) {
    Type *element_type = dynamic_cast<PointerType *>(operands[0]->get_type())
                             ->get_element_type();
    Type *inst_type;
    if (element_type->is_int_type())
        inst_type = parent->get_function()->get_module()->get_int_type();
    else if (element_type->is_float_type())
        inst_type = parent->get_function()->get_module()->get_float_type();
    else if (element_type->is_array_type())
        inst_type = parent->get_function()->get_module()->get_array_type(
            dynamic_cast<ArrayType *>(element_type)->get_element_type(),
            dynamic_cast<ArrayType *>(element_type)->get_length());
    return new LoadInst(inst_type, id, operands, parent);
}

StoreInst::StoreInst(Type *type, OpID id, vector<Value *> &operands,
                     BasicBlock *parent)
    : Instruction(type, id, 2, operands, parent) {}
Instruction *StoreInst::create(OpID id, std::vector<Value *> &&operands,
                               BasicBlock *parent) {
    return new StoreInst(parent->get_function()->get_module()->get_void_type(),
                         id, operands, parent);
}

CmpInst::CmpInst(Type *type, OpID id, vector<Value *> &operands,
                 BasicBlock *parent)
    : Instruction(type, id, 2, operands, parent) {}
Instruction *CmpInst::create(OpID id, std::vector<Value *> &&operands,
                             BasicBlock *parent) {
    return new CmpInst(parent->get_function()->get_module()->get_int_type(), id,
                       operands, parent);
}

FCmpInst::FCmpInst(Type *type, OpID id, vector<Value *> &operands,
                   BasicBlock *parent)
    : Instruction(type, id, 2, operands, parent) {}
Instruction *FCmpInst::create(OpID id, std::vector<Value *> &&operands,
                              BasicBlock *parent) {
    return new FCmpInst(parent->get_function()->get_module()->get_int_type(),
                        id, operands, parent);
}

CallInst::CallInst(Type *type, OpID id, vector<Value *> &operands,
                   BasicBlock *parent)
    : Instruction(type, id, operands.size(), operands, parent) {}
Instruction *CallInst::create(OpID id, std::vector<Value *> &&operands,
                              BasicBlock *parent) {
    return new CallInst(
        dynamic_cast<FuncType *>(operands[0]->get_type())->get_result_type(),
        id, operands, parent);
}

Fp2siInst::Fp2siInst(Type *type, OpID id, vector<Value *> &operands,
                     BasicBlock *parent)
    : Instruction(type, id, 1, operands, parent) {}
Instruction *Fp2siInst::create(OpID id, std::vector<Value *> &&operands,
                               BasicBlock *parent) {
    return new Fp2siInst(parent->get_function()->get_module()->get_int_type(),
                         id, operands, parent);
}

Si2fpInst::Si2fpInst(Type *type, OpID id, vector<Value *> &operands,
                     BasicBlock *parent)
    : Instruction(type, id, 1, operands, parent) {}
Instruction *Si2fpInst::create(OpID id, std::vector<Value *> &&operands,
                               BasicBlock *parent) {
    return new Si2fpInst(parent->get_function()->get_module()->get_float_type(),
                         id, operands, parent);
}