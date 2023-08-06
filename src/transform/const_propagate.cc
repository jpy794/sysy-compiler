#include "const_propagate.hh"
#include "constant.hh"
#include "err.hh"
#include "function.hh"
#include "instruction.hh"
#include "usedef_chain.hh"
#include "utils.hh"
#include "value.hh"
#include <stdexcept>

using namespace std;
using namespace pass;
using namespace ir;

void ConstPro::run(pass::PassManager *mgr) {
    auto m = mgr->get_module();
    for (auto &f_r : m->functions()) {
        {
            const_propa.clear();
            val2const.clear();
            work_list.clear();
        }
        traverse(&f_r);
        replace();
    }
}

void ConstPro::traverse(Function *func) {
    for (auto &bb_r : func->bbs()) {
        for (auto &inst_r : bb_r.insts()) {
            auto inst = &inst_r;
            if (check(inst)) {
                val2const[inst] = const_folder(inst);
                work_list.push_back(inst);
            }
        }
    }
}

void ConstPro::replace() {
    while (not work_list.empty()) {
        auto inst = work_list.front();
        work_list.pop_front();
        if (check(inst) and not contains(const_propa, inst)) {
            if (not contains(val2const, dynamic_cast<Value *>(inst)))
                val2const[inst] = const_folder(inst);
            for (auto &[user, _] : inst->get_use_list()) {
                work_list.push_back(as_a<Instruction>(user));
            }
            inst->replace_all_use_with(val2const[inst]);
            const_propa.insert(inst);
        }
    }
}

bool ConstPro::check(Instruction *inst) {
    if (not(is_a<PhiInst>(inst) || is_a<IBinaryInst>(inst) ||
            is_a<FBinaryInst>(inst) || is_a<ICmpInst>(inst) ||
            is_a<FCmpInst>(inst) || is_a<Fp2siInst>(inst) ||
            is_a<Si2fpInst>(inst) || is_a<ZextInst>(inst)))
        return false; // except for these instructions, other can't be folded
                      // constantly
    if (is_a<PhiInst>(inst)) {
        Constant *unqiue_const = nullptr;
        if (is_a<Constant>(inst->get_operand(0)))
            unqiue_const = as_a<Constant>(inst->get_operand(0));
        else if (contains(val2const, inst->get_operand(0)))
            unqiue_const = val2const[inst->get_operand(0)];
        else
            return false;
        for (unsigned i = 2; i < inst->operands().size(); i += 2) {
            if (is_a<Constant>(inst->get_operand(i))) {
                if (unqiue_const == as_a<Constant>(inst->get_operand(i)))
                    continue;
            } else if (contains(val2const, inst->get_operand(i))) {
                if (unqiue_const == val2const[inst->get_operand(i)])
                    continue;
            }
            return false;
        }
    } else {
        for (unsigned i = 0; i < inst->operands().size(); i++) {
            if (not(is_a<Constant>(inst->get_operand(i)) ||
                    contains(val2const, inst->get_operand(i))))
                return false;
        }
    }
    return true;
}

Constant *ConstPro::const_folder(Instruction *inst) {
    if (is_a<PhiInst>(inst)) {
        return get_const(as_a<PhiInst>(inst)->get_operand(0));
    } else if (is_a<IBinaryInst>(inst)) {
        if (as_a<IBinaryInst>(inst)->get_ibin_op() == IBinaryInst::XOR) {
            auto l_val =
                as_a<ConstBool>(get_const(inst->get_operand(0)))->val();
            auto r_val =
                as_a<ConstBool>(get_const(inst->get_operand(1)))->val();
            return Constants::get().bool_const(l_val ^ r_val);
        }
        auto l_val = as_a<ConstInt>(get_const(inst->get_operand(0)))->val();
        auto r_val = as_a<ConstInt>(get_const(inst->get_operand(1)))->val();
        switch (as_a<IBinaryInst>(inst)->get_ibin_op()) {
        case IBinaryInst::ADD:
            return Constants::get().int_const(l_val + r_val);
        case IBinaryInst::SUB:
            return Constants::get().int_const(l_val - r_val);
        case IBinaryInst::MUL:
            return Constants::get().int_const(l_val * r_val);
        case IBinaryInst::SDIV:
            return Constants::get().int_const(l_val / r_val);
        case IBinaryInst::SREM:
            return Constants::get().int_const(l_val % r_val);
        case IBinaryInst::XOR:
            throw unreachable_error();
        }
    } else if (is_a<FBinaryInst>(inst)) {
        auto l_val = as_a<ConstFloat>(get_const(inst->get_operand(0)))->val();
        auto r_val = as_a<ConstFloat>(get_const(inst->get_operand(1)))->val();
        switch (as_a<FBinaryInst>(inst)->get_fbin_op()) {
        case FBinaryInst::FADD:
            return Constants::get().float_const(l_val + r_val);
        case FBinaryInst::FSUB:
            return Constants::get().float_const(l_val - r_val);
        case FBinaryInst::FMUL:
            return Constants::get().float_const(l_val * r_val);
        case FBinaryInst::FDIV:
            return Constants::get().float_const(l_val / r_val);
            // TODO: if FBinarayInst have FREM, this case need to be added here
        }
    } else if (is_a<ICmpInst>(inst)) {
        auto l_val = as_a<ConstInt>(get_const(inst->get_operand(0)))->val();
        auto r_val = as_a<ConstInt>(get_const(inst->get_operand(1)))->val();
        switch (as_a<ICmpInst>(inst)->get_icmp_op()) {
        case ICmpInst::EQ:
            return Constants::get().bool_const(l_val == r_val);
        case ICmpInst::NE:
            return Constants::get().bool_const(l_val != r_val);
        case ICmpInst::GT:
            return Constants::get().bool_const(l_val > r_val);
        case ICmpInst::GE:
            return Constants::get().bool_const(l_val >= r_val);
        case ICmpInst::LT:
            return Constants::get().bool_const(l_val < r_val);
        case ICmpInst::LE:
            return Constants::get().bool_const(l_val <= r_val);
        }
    } else if (is_a<FCmpInst>(inst)) {
        auto l_val = as_a<ConstFloat>(get_const(inst->get_operand(0)))->val();
        auto r_val = as_a<ConstFloat>(get_const(inst->get_operand(1)))->val();
        switch (as_a<FCmpInst>(inst)->get_fcmp_op()) {
        case FCmpInst::FEQ:
            return Constants::get().bool_const(l_val == r_val);
        case FCmpInst::FNE:
            return Constants::get().bool_const(l_val != r_val);
        case FCmpInst::FGT:
            return Constants::get().bool_const(l_val > r_val);
        case FCmpInst::FGE:
            return Constants::get().bool_const(l_val >= r_val);
        case FCmpInst::FLT:
            return Constants::get().bool_const(l_val < r_val);
        case FCmpInst::FLE:
            return Constants::get().bool_const(l_val <= r_val);
        }
    } else if (is_a<Fp2siInst>(inst)) {
        auto val = as_a<ConstFloat>(get_const(inst->get_operand(0)))->val();
        auto si_val = (int)(val);
        return Constants::get().int_const(si_val);
    } else if (is_a<Si2fpInst>(inst)) {
        auto val = as_a<ConstInt>(get_const(inst->get_operand(0)))->val();
        auto fp_val = (int)(val);
        return Constants::get().float_const(fp_val);
    } else if (is_a<ZextInst>(inst)) {
        auto val = as_a<ConstBool>(get_const(inst->get_operand(0)))->val();
        auto zext_val = (int)(val);
        return Constants::get().int_const(zext_val);
    } else {
        throw logic_error{inst->get_type()->print() +
                          " can't be folded constantly"};
    }
}

Constant *ConstPro::get_const(Value *val) {
    if (is_a<Constant>(val))
        return as_a<Constant>(val);
    else if (contains(val2const, val))
        return val2const[val];
    else
        throw logic_error{"this inst can't be constant"};
}
