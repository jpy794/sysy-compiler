#pragma once

#include <any>

namespace ir {

class RetInst;
class BrInst;
class IBinaryInst;
class FBinaryInst;
class AllocaInst;
class LoadInst;
class StoreInst;
class ICmpInst;
class FCmpInst;
class PhiInst;
class CallInst;
class Fp2siInst;
class Si2fpInst;
class GetElementPtrInst;
class ZextInst;
class SextInst;
class Ptr2IntInst;
class Int2PtrInst;
class TruncInst;

class InstructionVisitor {
  public:
    virtual std::any visit(const RetInst *instruction) = 0;
    virtual std::any visit(const BrInst *instruction) = 0;
    virtual std::any visit(const IBinaryInst *instruction) = 0;
    virtual std::any visit(const FBinaryInst *instruction) = 0;
    virtual std::any visit(const AllocaInst *instruction) = 0;
    virtual std::any visit(const LoadInst *instruction) = 0;
    virtual std::any visit(const StoreInst *instruction) = 0;
    virtual std::any visit(const ICmpInst *instruction) = 0;
    virtual std::any visit(const FCmpInst *instruction) = 0;
    virtual std::any visit(const PhiInst *instruction) = 0;
    virtual std::any visit(const CallInst *instruction) = 0;
    virtual std::any visit(const Fp2siInst *instruction) = 0;
    virtual std::any visit(const Si2fpInst *instruction) = 0;
    virtual std::any visit(const GetElementPtrInst *instruction) = 0;
    virtual std::any visit(const ZextInst *instruction) = 0;
    virtual std::any visit(const SextInst *instruction) = 0;
    virtual std::any visit(const Ptr2IntInst *instruction) = 0;
    virtual std::any visit(const Int2PtrInst *instruction) = 0;
    virtual std::any visit(const TruncInst *instruction) = 0;
};

} // namespace ir
