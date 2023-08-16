#include "strength_reduce.hh"
#include "instruction.hh"
#include "pass.hh"
#include "utils.hh"
#include <cassert>
#include <vector>

using namespace ir;
using namespace pass;
using namespace std;

// TODO set changed
bool StrengthReduce::run(PassManager *mgr) {
    changed = false;
    for (auto &f_r : mgr->get_module()->functions()) {
        if (f_r.is_external)
            continue;
        // combine_continuous_add(&f_r);
        // algebraic_combine(&f_r);
    }
    return changed;
}

void StrengthReduce::combine_continuous_add(Function *func) {
    auto parese_add_const = [](Instruction *inst) {
        struct {
            Value *src{nullptr};
            int const_v{-1};
        } ret;

        if (not(is_a<IBinaryInst>(inst) and
                as_a<IBinaryInst>(inst)->get_ibin_op() == IBinaryInst::ADD))
            return ret;
        if (is_a<ConstInt>(inst->get_operand(0)) or
            is_a<ConstInt>(inst->get_operand(1))) {
            unsigned const_idx = is_a<ConstInt>(inst->get_operand(0)) ? 0 : 1;

            ret.const_v = as_a<ConstInt>(inst->get_operand(const_idx))->val();
            ret.src = inst->get_operand(1 - const_idx);
        }
        return ret;
    };

    for (auto &bb_r : func->bbs()) {
        for (auto &inst_r : bb_r.insts()) {
            auto inst = &inst_r;

            Value *new_op0 = nullptr;
            int const_op1 = 0;

            auto new_op = parese_add_const(inst);
            while (new_op.src) {
                new_op0 = new_op.src;
                const_op1 += new_op.const_v;

                if (not is_a<Instruction>(new_op.src))
                    break;
                new_op = parese_add_const(as_a<Instruction>(new_op.src));
            }
            if (new_op0) {
                inst->set_operand(0, new_op0);
                inst->set_operand(1, Constants::get().int_const(const_op1));
            }
        }
    }
}

struct AddTree {
    bool check_ok{false};
    vector<IBinaryInst *> add_group;
    vector<IBinaryInst *> leaves;

    // leaf feature
    struct {
        IBinaryInst::IBinOp binop;
        Value *op{nullptr};
    } common;
    vector<Value *> sources;

    AddTree(Instruction *inst) {
        check_ok = extend(inst) and add_group.size() and check_leaf();
    };

    bool check_leaf() {
        assert(leaves.size() >= 2);
        auto leaf0 = as_a<IBinaryInst>(leaves[0]);
        auto leaf1 = as_a<IBinaryInst>(leaves[1]);

        if (leaf0->get_ibin_op() != leaf1->get_ibin_op())
            return false;

        common.binop = leaf0->get_ibin_op();
        bool commutative = common.binop == IBinaryInst::MUL;

        auto init_helper = [&](int same_idx, pair<int, int> src_idx) {
            assert(same_idx != src_idx.first);
            common.op = leaf0->get_operand(same_idx);
            sources.push_back(leaf0->get_operand(src_idx.first));
            sources.push_back(leaf1->get_operand(src_idx.second));
        };

        if (commutative) {
            if (leaf0->get_operand(0) == leaf1->get_operand(0)) {
                init_helper(0, {1, 1});
            } else if (leaf0->get_operand(0) == leaf1->get_operand(1)) {
                init_helper(0, {1, 0});
            } else if (leaf0->get_operand(1) == leaf1->get_operand(0)) {
                init_helper(1, {0, 1});
            } else if (leaf0->get_operand(1) == leaf1->get_operand(1)) {
                init_helper(1, {0, 0});
            } else
                return false;
        } else {
            if (leaf0->get_operand(1) != leaf1->get_operand(1))
                return false;
            init_helper(1, {0, 0});
        }

        for (unsigned i = 2; i < leaves.size(); ++i) {
            if (leaves[i]->get_ibin_op() != common.binop)
                return false;
            auto op0 = leaves[i]->get_operand(0);
            auto op1 = leaves[i]->get_operand(1);
            if (op1 == common.op)
                sources.push_back(op0);
            else if (op0 == common.op and commutative)
                sources.push_back(op1);
            else
                return false;
        }
        assert(leaves.size() == sources.size());

        return true;
    }

    bool extend(Instruction *inst) {
        if (not is_a<IBinaryInst>(inst))
            return false;

        auto bi_inst = as_a<IBinaryInst>(inst);
        switch (bi_inst->get_ibin_op()) {
        case IBinaryInst::ADD:
            add_group.push_back(bi_inst);
            break;
        case IBinaryInst::MUL:
        case IBinaryInst::SDIV:
        case IBinaryInst::SREM:
            leaves.push_back(bi_inst);
            return true;
        default:
            return false;
            break;
        }

        for (auto op : inst->operands()) {
            if (not is_a<Instruction>(op) or not extend(as_a<Instruction>(op)))
                return false;
        }

        return true;
    }
};

void StrengthReduce::algebraic_combine(ir::Function *func) {
    set<Instruction *> marked;
    for (auto &bb_r : func->bbs()) {
        auto &insts = bb_r.insts();
        for (auto inst_iter = insts.rbegin(); inst_iter != insts.rend();
             ++inst_iter) {
            auto inst = &*inst_iter;
            if (contains(marked, inst))
                continue;
            AddTree tree(inst);
            if (not tree.check_ok)
                continue;

            auto new_inst = bb_r.insert_inst<IBinaryInst>(
                inst, IBinaryInst::ADD, tree.sources[0], tree.sources[1]);

            for (unsigned i = 2; i < tree.sources.size(); ++i) {
                new_inst = bb_r.insert_inst<IBinaryInst>(
                    inst, IBinaryInst::ADD, new_inst, tree.sources[i]);
            }
            new_inst = bb_r.insert_inst<IBinaryInst>(inst, tree.common.binop,
                                                     new_inst, tree.common.op);
            inst->replace_all_use_with(new_inst);

            for (auto add : tree.add_group)
                marked.insert(add);
            for (auto leaf : tree.leaves)
                marked.insert(leaf);

            changed = true;
        }
    }
}
