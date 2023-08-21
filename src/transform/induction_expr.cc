#include "induction_expr.hh"
#include "instruction.hh"
#include "loop_find.hh"
#include "user.hh"
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
    _func_loops = &mgr->get_result<LoopFind>();
    changed = false;
    replace_table.clear();
    for (auto &f_r : m->functions()) {
        for (auto &bb_r : f_r.bbs()) {
            for (auto &inst_r : bb_r.insts()) {
                if (is_induction_expr(&inst_r)) {
                    if (get_iter_times(&inst_r) == nullptr) {
                        continue;
                    }
                    for (auto &use : inst_r.get_use_list()) {
                        // induction_expr is used out of the loop
                        auto user_inst = as_a<Instruction>(use.user);
                        if (out_of_loop(user_inst->get_parent(),
                                        inst_r.get_parent()) &&
                            not is_a<PhiInst>(user_inst)) {
                            induction(&inst_r, use);
                        }
                    }
                }
            }
        }
    }
    // replace use of expr with new val;
    for (auto [val, use] : replace_table) {
        use.first->set_operand(use.second, val);
    }
    return changed;
}

void InductionExpr::induction(ir::Instruction *expr, const ir::Use &use) {
    // execute induction rules as follow
    auto new_val = induce_add_rem(expr, as_a<Instruction>(use.user));
    if (new_val)
        replace_table[new_val] = {use.user, use.op_idx};
}

// expr = ( expr + n ) % m -> init%m + (n%m*iter_times%m)%m
Value *InductionExpr::induce_add_rem(ir::Instruction *expr,
                                     ir::Instruction *user) {
    auto l_val = expr->get_operand(0);
    auto r_val = expr->get_operand(2);
    Value *init_val;
    Value *rem;
    Instruction *add;
    // detect which val is rem_bin
    if (is_a<IBinaryInst>(l_val) &&
        as_a<IBinaryInst>(l_val)->get_ibin_op() == IBinaryInst::SREM) {
        init_val = r_val;
        rem = as_a<IBinaryInst>(l_val)->get_operand(1);
        add = as_a<IBinaryInst>(l_val)->get_operand(0)->as<Instruction>();
    } else if (is_a<IBinaryInst>(r_val) &&
               as_a<IBinaryInst>(r_val)->get_ibin_op() == IBinaryInst::SREM) {
        init_val = l_val;
        rem = as_a<IBinaryInst>(r_val)->get_operand(1);
        add = as_a<IBinaryInst>(r_val)->get_operand(0)->as<Instruction>();
    } else
        return nullptr;
    auto iter_times = get_iter_times(expr);
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
        return nullptr;
    if (is_a<Instruction>(add_val) &&
        not out_of_loop(as_a<Instruction>(add_val)->get_parent(),
                        expr->get_parent()))
        return nullptr;
    auto n_m = user->get_parent()->insert_inst<IBinaryInst>(
        user, IBinaryInst::SREM, add_val, rem);
    auto n_m_64 = user->get_parent()->insert_inst<SextInst>(user, n_m);
    auto iter_m_64 = user->get_parent()->insert_inst<SextInst>(user, iter_m);
    auto rem_64 = user->get_parent()->insert_inst<SextInst>(user, rem);
    auto mul_64 = user->get_parent()->insert_inst<IBinaryInst>(
        user, IBinaryInst::MUL, n_m_64, iter_m_64);
    auto mul_m_64 = user->get_parent()->insert_inst<IBinaryInst>(
        user, IBinaryInst::SREM, mul_64, rem_64);
    auto mul_m = user->get_parent()->insert_inst<TruncInst>(user, mul_m_64);
    auto res = user->get_parent()->insert_inst<IBinaryInst>(
        user, IBinaryInst::ADD, init_m, mul_m);
    return res;
}