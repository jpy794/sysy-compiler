#include "induction_expr.hh"
#include "instruction.hh"
#include "utils.hh"

using namespace std;
using namespace pass;
using namespace ir;

inline bool InductionExpr::is_induction_expr(Instruction *inst) {
    if (not is_a<PhiInst>(inst))
        return false;
    if (not is_loop_head(inst->get_parent()))
        return false;
    // don't address the case with more than 2 pre_bbs
    if (as_a<PhiInst>(inst)->operands().size() > 4)
        return false;
    return true;
}

bool InductionExpr::run(pass::PassManager *mgr) {
    auto m = mgr->get_module();
    bool changed = false;
    for (auto &f_r : m->functions()) {
        for (auto &bb_r : f_r.bbs()) {
            for (auto &inst_r : bb_r.insts()) {
                if (is_induction_expr(&inst_r)) {
                    for (auto &use : inst_r.get_use_list()) {
                        // induction_expr is used out of the loop
                        auto user_inst = as_a<Instruction>(use.user);
                        if (out_of_loop(user_inst->get_parent(),
                                        inst_r.get_parent())) {
                            changed |= induction(&inst_r, user_inst);
                        }
                    }
                }
            }
        }
    }
    return changed;
}

bool InductionExpr::induction(ir::Instruction *expr, ir::Instruction *user) {
    bool changed = false;
    changed |= add_rem(expr, user);
    return changed;
}

// expr = ( expr + n ) % m -> init%m + (n%m*iter_times%m)%m
bool InductionExpr::add_rem(ir::Instruction *expr, ir::Instruction *user) {
    auto l_val = expr->get_operand(0);
    auto r_val = expr->get_operand(2);
    Value *init_val;
    Value *rem;
    // detect which val is rem_bin
    if (is_a<IBinaryInst>(l_val) &&
        as_a<IBinaryInst>(l_val)->get_ibin_op() == IBinaryInst::SREM) {
        init_val = r_val;
        rem = as_a<IBinaryInst>(l_val)->get_operand(1);
    } else if (is_a<IBinaryInst>(r_val) &&
               as_a<IBinaryInst>(r_val)->get_ibin_op() == IBinaryInst::SREM) {
        init_val = l_val;
        rem = as_a<IBinaryInst>(r_val)->get_operand(1);
    } else
        return false;
    auto iter_times = get_iter_times(expr);
    auto add = as_a<IBinaryInst>(r_val)->get_operand(0)->as<Instruction>();
    auto init_m = user->get_parent()->insert_inst<IBinaryInst>(
        user, IBinaryInst::SREM, init_val, rem);
    auto iter_m = user->get_parent()->insert_inst<IBinaryInst>(
        user, IBinaryInst::SREM, iter_times, rem);
    Value *add_val;
    // detect which oper is the expr
    if (add->get_operand(0) == expr) {
        add_val = add->get_operand(1);
    } else if (add->get_operand(1) == expr) {
        add_val = add->get_operand(0);
    } else
        return false;
    auto n_m = user->get_parent()->insert_inst<IBinaryInst>(
        user, IBinaryInst::SREM, add_val, rem);
    auto mul = user->get_parent()->insert_inst<IBinaryInst>(
        user, IBinaryInst::MUL, n_m,
        iter_m); // FIXME: use 64b register
    auto mul_m = user->get_parent()->insert_inst<IBinaryInst>(
        user, IBinaryInst::SREM, mul,
        rem); // FIXME: use 64b register and store into 32b register
    auto res = user->get_parent()->insert_inst<IBinaryInst>(
        user, IBinaryInst::ADD, init_m, mul_m);
    user->replace_all_use_with(res);
    return true;
}