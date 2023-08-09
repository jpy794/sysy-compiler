#include "continuous_addition.hh"
#include "constant.hh"
#include "err.hh"
#include "function.hh"
#include "instruction.hh"
#include "utils.hh"

using namespace std;
using namespace pass;
using namespace ir;

void ContinuousAdd::run(pass::PassManager *mgr) {
    auto m = mgr->get_module();
    for (auto &f_r : m->functions()) {
        if (f_r.is_external)
            continue;
        add_table.clear();
        scan(&f_r);
        add2mul(&f_r);
    }
}
void ContinuousAdd::scan(Function *func) {
    for (auto &bb_r : func->bbs()) {
        for (auto &inst_r : bb_r.insts()) {
            if (is_a<IBinaryInst>(&inst_r)) {
                auto inst = as_a<IBinaryInst>(&inst_r);
                if (inst->get_ibin_op() == IBinaryInst::ADD) {
                    create_add_con(inst);
                }
            } else if (is_a<FBinaryInst>(&inst_r)) {
                auto inst = as_a<FBinaryInst>(&inst_r);
                if (inst->get_fbin_op() == FBinaryInst::FADD) {
                    create_add_con(inst);
                }
            }
        }
    }
}

void ContinuousAdd::create_add_con(Instruction *inst) {
    auto lhs = inst->get_operand(0);
    auto rhs = inst->get_operand(1);
    // Case1:lhs is continuous addition
    if (contains(add_table, lhs)) {
        if (add_table[lhs].addend == rhs) {
            add_table[inst] = {add_table[lhs].base, rhs,
                               add_table[lhs].times + 1};
            return;
        }
        // it can't tell which addend is base at the beginning
        else if (add_table[lhs].times == 1 && add_table[lhs].base == rhs) {
            add_table[inst] = {add_table[lhs].addend, rhs, 2};
            return;
        }
        // maybe lhs is a continuous_addition but it's not the
        // one that inst need
    }
    // Case2:rhs is continuous addition as same as Case1
    if (contains(add_table, rhs)) {
        if (add_table[rhs].addend == lhs) {
            add_table[inst] = {add_table[rhs].base, lhs,
                               add_table[rhs].times + 1};
            return;
        } else if (add_table[rhs].times == 1 && add_table[rhs].base == lhs) {
            add_table[inst] = {add_table[rhs].addend, lhs, 2};
            return;
        }
    }
    // Case3:for this addition, neither lhs nor rhs is the
    // addend of continuum
    if (lhs == rhs) {
        add_table[inst] = {nullptr, lhs, 2};
    } else {
        add_table[inst] = {lhs, rhs, 1};
    }
}

void ContinuousAdd::add2mul(Function *func) {
    for (auto &[val, con] : add_table) {
        if (con.times > add_upper_times) {
            auto inst = as_a<Instruction>(val);
            auto bb = inst->get_parent();
            if (is_a<IBinaryInst>(val)) {
                auto product = bb->insert_inst<IBinaryInst>(
                    inst, IBinaryInst::MUL, con.addend,
                    Constants::get().int_const(con.times));
                IBinaryInst *res = product;
                if (con.base != nullptr)
                    res = bb->insert_inst<IBinaryInst>(inst, IBinaryInst::ADD,
                                                       con.base, product);
                inst->replace_all_use_with(res);
            } else if (is_a<FBinaryInst>(val)) {
                auto product = bb->insert_inst<FBinaryInst>(
                    inst, FBinaryInst::FMUL, con.addend,
                    Constants::get().float_const(con.times));
                FBinaryInst *res = product;
                if (con.base != nullptr)
                    res = bb->insert_inst<FBinaryInst>(inst, FBinaryInst::FADD,
                                                       con.base, product);
                inst->replace_all_use_with(res);
            } else {
                throw unreachable_error{};
            }
        }
    }
}