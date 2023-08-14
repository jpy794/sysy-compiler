#include "local_cmnexpr.hh"
#include "depth_order.hh"
#include "err.hh"
#include "instruction.hh"
#include "utils.hh"
#include <iostream>
#include <utility>

using namespace std;
using namespace ir;
using namespace pass;

bool LocalCmnExpr::run(pass::PassManager *mgr) {
    changed = false;
    auto m = mgr->get_module();
    depth_order = &mgr->get_result<DepthOrder>();
    for (auto &func_r : m->functions()) {
        if (func_r.is_external)
            continue;
        for (auto bb : depth_order->_depth_priority_order.at(&func_r)) {
            cmn_expr.clear();
            for (auto &inst_r : bb->insts()) {
                auto inst = &inst_r;
                if (check_inst(inst)) {
                    auto op = get_op(inst);
                    pair<OP, const std::vector<Value *>> hash_pair =
                        make_pair(op, inst->operands());
                    // if there is a common expression in cmn_expr, then replace
                    // all uses with the mapped inst
                    if (contains(cmn_expr, {op, inst->operands()})) {
                        inst->replace_all_use_with(
                            cmn_expr[{op, inst->operands()}]);
                        changed = true;
                    }
                    // create a new value expr
                    else {
                        cmn_expr[{op, inst->operands()}] = inst;
                    }
                }
            }
        }
    }
    return changed;
}

bool LocalCmnExpr::check_inst(Instruction *inst) {
    return not(is_a<ir::AllocaInst>(inst) || is_a<ir::BrInst>(inst) ||
               is_a<ir::PhiInst>(inst) || is_a<ir::RetInst>(inst) ||
               is_a<ir::LoadInst>(inst) || is_a<ir::StoreInst>(inst) ||
               is_a<ir::CallInst>(inst));
}

LocalCmnExpr::OP LocalCmnExpr::get_op(Instruction *inst) {
    if (is_a<IBinaryInst>(inst)) {
        switch (as_a<IBinaryInst>(inst)->get_ibin_op()) {
        case IBinaryInst::ADD:
            return OP::ADD;
        case IBinaryInst::SUB:
            return OP::SUB;
        case IBinaryInst::MUL:
            return OP::MUL;
        case IBinaryInst::SDIV:
            return OP::SDIV;
        case IBinaryInst::SREM:
            return OP::SREM;
        case IBinaryInst::XOR:
            return OP::XOR;
        case IBinaryInst::LSHR:
            return OP::LSHR;
        case IBinaryInst::ASHR:
            return OP::ASHR;
        case IBinaryInst::SHL:
            return OP::SHL;
        }

    } else if (is_a<ICmpInst>(inst)) {
        switch (as_a<ICmpInst>(inst)->get_icmp_op()) {
        case ICmpInst::EQ:
            return OP::EQ;
        case ICmpInst::NE:
            return OP::NE;
        case ICmpInst::GT:
            return OP::GT;
        case ICmpInst::GE:
            return OP::GE;
        case ICmpInst::LT:
            return OP::LT;
        case ICmpInst::LE:
            return OP::LE;
        }
    } else if (is_a<GetElementPtrInst>(inst)) {
        return OP::GEP;
    } else if (is_a<ZextInst>(inst)) {
        return OP::ZEXT;
    } else if (is_a<FBinaryInst>(inst)) {
        switch (as_a<FBinaryInst>(inst)->get_fbin_op()) {
        case FBinaryInst::FADD:
            return OP::FADD;
        case FBinaryInst::FSUB:
            return OP::FSUB;
        case FBinaryInst::FMUL:
            return OP::FMUL;
        case FBinaryInst::FDIV:
            return OP::FDIV;
        }
    } else if (is_a<FCmpInst>(inst)) {
        switch (as_a<FCmpInst>(inst)->get_fcmp_op()) {
        case FCmpInst::FEQ:
            return OP::FEQ;
        case FCmpInst::FNE:
            return OP::FNE;
        case FCmpInst::FGT:
            return OP::FGT;
        case FCmpInst::FGE:
            return OP::FGE;
        case FCmpInst::FLT:
            return OP::FLT;
        case FCmpInst::FLE:
            return OP::FLE;
        }
    } else if (is_a<Si2fpInst>(inst)) {
        return OP::SI2FP;
    } else if (is_a<Fp2siInst>(inst)) {
        return OP::FP2SI;
    } else {
        throw unreachable_error{};
    }
}