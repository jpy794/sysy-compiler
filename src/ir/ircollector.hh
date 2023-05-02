#include "basic_block.hh"
#include "constant.hh"
#include "err.hh"
#include "function.hh"
#include "instruction.hh"
#include "module.hh"
#include "type.hh"
#include "value.hh"

#include <cassert>
#include <optional>
#include <utility>
#include <vector>

namespace ir {

/* IRCollector
 * - An capsulation class for instruction construction
 * - manage the logic for insertion of ir:
 *   - which bb to insert
 *   - parent bb has an instr-list, which point should ir be inserted into
 * -
 */
// TODO:
// - add instruction:
//   - logic binary
//   - unary
class IRCollector {
    enum InsertMode { Back, Front, DIY };

  private:
    Module *_m;
    BasicBlock *_cur_bb{nullptr};

  public:
    IRCollector(const IRCollector &) = delete;
    IRCollector(IRCollector &&) = delete;
    explicit IRCollector(Module *m) : _m(m) {}
    void set_insertion(BasicBlock *bb) { _cur_bb = bb; }
    BasicBlock *get_insertion(BasicBlock *bb) { return _cur_bb; }

    RetInst *create_ret(std::optional<Value *> ret_val, InsertMode im = Back) {
        RetInst *ret{nullptr};
        if (ret_val.has_value())
            ret = new RetInst(_cur_bb, {ret_val.value()});
        else
            ret = new RetInst(_cur_bb, {});
        insert(ret, im);
        return ret;
    }

    BrInst *create_br(BasicBlock *nextbb, InsertMode im = Back) {
        std::vector<Value *> ops;
        auto ret = create_br_({nextbb});
        insert(ret, im);
        return ret;
    }
    BrInst *create_con_br(Value *cond, BasicBlock *TBB, BasicBlock *FBB,
                          InsertMode im = Back) {
        auto ret = create_br_({cond, TBB, FBB});
        insert(ret, im);
        return ret;
    }

    BinaryInst *create_add(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_binary(i32_type(), BinaryInst::ADD, lhs, rhs);
        insert(ret, im);
        return ret;
    }
    BinaryInst *create_sub(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_binary(i32_type(), BinaryInst::SUB, lhs, rhs);
        insert(ret, im);
        return ret;
    }
    BinaryInst *create_mul(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_binary(i32_type(), BinaryInst::MUL, lhs, rhs);
        insert(ret, im);
        return ret;
    }
    BinaryInst *create_sdiv(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_binary(i32_type(), BinaryInst::SDIV, lhs, rhs);
        insert(ret, im);
        return ret;
    }
    BinaryInst *create_srem(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_binary(i32_type(), BinaryInst::SREM, lhs, rhs);
        insert(ret, im);
        return ret;
    }
    BinaryInst *create_fadd(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_binary(float_type(), BinaryInst::FADD, lhs, rhs);
        insert(ret, im);
        return ret;
    }
    BinaryInst *create_fsub(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_binary(float_type(), BinaryInst::FSUB, lhs, rhs);
        insert(ret, im);
        return ret;
    }
    BinaryInst *create_fmul(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_binary(float_type(), BinaryInst::FMUL, lhs, rhs);
        insert(ret, im);
        return ret;
    }
    BinaryInst *create_fdiv(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_binary(float_type(), BinaryInst::FDIV, lhs, rhs);
        insert(ret, im);
        return ret;
    }
    BinaryInst *create_frem(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_binary(float_type(), BinaryInst::FREM, lhs, rhs);
        insert(ret, im);
        return ret;
    }

    AllocaInst *create_alloc(Type *ele_ty, InsertMode im = Back) {
        // %op = i32* alloca i32
        // we pass the i32* into constructor, and i32 can be judged from i32*
        auto ret = new AllocaInst(_cur_bb, {}, ele_ty);
        insert(ret, im);
        return ret;
    }

    // user should be clear what type he wants from LoadInst
    LoadInst *create_load(Type *type, Value *ptr, InsertMode im = Back) {
        auto ptr_type = dynamic_cast<const PointerType *>(ptr->get_type());
        assert(ptr_type && ptr_type->get_element_type() == type);
        auto ret = new LoadInst(_cur_bb, {ptr});
        insert(ret, im);
        return ret;
    }

    StoreInst *create_store(Value *value, Value *ptr, InsertMode im = Back) {
        auto ptr_type = dynamic_cast<const PointerType *>(ptr->get_type());
        assert(ptr_type);
        assert(ptr_type->get_element_type() == value->get_type());
        auto ret = new StoreInst(_cur_bb, {value, ptr});
        insert(ret, im);
        return ret;
    }

    CmpInst *create_eq(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_cmp(CmpInst::EQ, lhs, rhs);
        insert(ret, im);
        return ret;
    }
    CmpInst *create_ne(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_cmp(CmpInst::NE, lhs, rhs);
        insert(ret, im);
        return ret;
    }
    CmpInst *create_gt(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_cmp(CmpInst::GT, lhs, rhs);
        insert(ret, im);
        return ret;
    }
    CmpInst *create_ge(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_cmp(CmpInst::GE, lhs, rhs);
        insert(ret, im);
        return ret;
    }
    CmpInst *create_lt(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_cmp(CmpInst::LT, lhs, rhs);
        insert(ret, im);
        return ret;
    }
    CmpInst *create_le(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_cmp(CmpInst::LE, lhs, rhs);
        insert(ret, im);
        return ret;
    }

    CmpInst *create_feq(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_fcmp(CmpInst::FEQ, lhs, rhs);
        insert(ret, im);
        return ret;
    }
    CmpInst *create_fne(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_fcmp(CmpInst::FNE, lhs, rhs);
        insert(ret, im);
        return ret;
    }
    CmpInst *create_fgt(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_fcmp(CmpInst::FGT, lhs, rhs);
        insert(ret, im);
        return ret;
    }
    CmpInst *create_fge(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_fcmp(CmpInst::FGE, lhs, rhs);
        insert(ret, im);
        return ret;
    }
    CmpInst *create_flt(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_fcmp(CmpInst::FLT, lhs, rhs);
        insert(ret, im);
        return ret;
    }
    CmpInst *create_fle(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = create_fcmp(CmpInst::FLE, lhs, rhs);
        insert(ret, im);
        return ret;
    }

    PhiInst *create_phi(Value *lhs, Value *rhs, InsertMode im = Back) {
        auto ret = new PhiInst(
            _cur_bb, {lhs, dynamic_cast<Instruction *>(lhs)->get_parent(), rhs,
                      dynamic_cast<Instruction *>(rhs)->get_parent()});
        insert(ret, im);
        return ret;
    }

    CallInst *create_call(Function *func, std::vector<Value *> &args,
                          InsertMode im = Back) {
        std::vector<Value *> ops{args};
        ops.insert(ops.begin(), func);
        auto ret = new CallInst(_cur_bb, static_cast<decltype(ops) &&>(ops));
        insert(ret, im);
        return ret;
    }

    GetElementPtrInst *create_gep(Value *ptr, std::vector<Value *> idx,
                                  InsertMode im = Back) {
        if (!dynamic_cast<Argument *>(idx[0]))
            idx.insert(idx.cbegin(), _m->be_cached(0));
        if (dynamic_cast<ArrayType *>(ptr->get_type())->get_dims().size() !=
            idx.size() - 1)
            idx.push_back(_m->be_cached(0));
        return new GetElementPtrInst(_cur_bb, std::move(idx));
    }

    Fp2siInst *create_fp2si(Value *value, InsertMode im = Back) {
        assert(value->get_type()->is_int_type());
        auto ret = new Fp2siInst(_cur_bb, {value});
        insert(ret, im);
        return ret;
    }

    Si2fpInst *create_si2fp(Value *value, InsertMode im = Back) {
        assert(value->get_type()->is_float_type());
        auto ret = new Si2fpInst(_cur_bb, {value});
        insert(ret, im);
        return ret;
    }

    ZextInst *create_zext(Value *value, InsertMode im = Back) {
        assert(dynamic_cast<IntType *>(value->get_type())->get_num_bits() == 1);
        auto ret = new ZextInst(_cur_bb, {value});
        insert(ret, im);
        return ret;
    }

  private:
    inline Type *void_type() const { return _m->get_void_type(); }
    inline Type *i32_type() const { return _m->get_int32_type(); }
    inline Type *i1_type() const { return _m->get_int1_type(); }
    inline Type *float_type() const { return _m->get_float_type(); }
    inline Type *ptr_type(Type *ele_ty) const {
        return _m->get_pointer_type(ele_ty);
    }

    void insert(Instruction *ins, InsertMode im) {
        switch (im) {
        case Back:
            _cur_bb->get_instructions().push_back(ins);
            break;
        case Front:
            _cur_bb->get_instructions().push_front(ins);
            break;
        case DIY:
#ifdef __DEBUG__
            std::cout
                << "warning: using diy mode for new instruction, u should take "
                   "care of it\n";
#endif
            break;
        }
    }

    // Below are helper functions, which are not reponsible for the insertion
    // logic
    BrInst *create_br_(std::vector<Value *> &&ops) {
        return new BrInst(_cur_bb, static_cast<decltype(ops) &&>(ops));
    }

    BinaryInst *create_binary(Type *type, BinaryInst::BinOp opid, Value *lhs,
                              Value *rhs) {
        assert(type->is_int_type() or type->is_float_type());
        assert(type == lhs->get_type());
        assert(type == rhs->get_type());
        return new BinaryInst(_cur_bb, {lhs, rhs}, opid);
    }
    CmpInst *create_cmp(CmpInst::CmpOp id, Value *lhs, Value *rhs) {
        assert(lhs->get_type()->is_int_type());
        assert(rhs->get_type()->is_int_type());
        return new CmpInst(_cur_bb, {lhs, rhs}, id);
    }
    CmpInst *create_fcmp(CmpInst::CmpOp id, Value *lhs, Value *rhs) {
        assert(lhs->get_type()->is_float_type());
        assert(rhs->get_type()->is_float_type());
        return new CmpInst(_cur_bb, {lhs, rhs}, id);
    }
};
}; // namespace ir
