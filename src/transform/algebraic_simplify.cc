#include "algebraic_simplify.hh"
#include "basic_block.hh"
#include "const_propagate.hh"
#include "constant.hh"
#include "dead_code.hh"
#include "instruction.hh"
#include "ir_matcher.hh"
#include "pass.hh"
#include "type.hh"
#include "value.hh"
#include <any>
#include <cassert>
#include <type_traits>

using namespace pass;
using namespace ir;
using namespace Matcher;

Constant *get_cint(int v, bool i64) {
    if (i64)
        return Constants::get().i64_const(v);
    else
        return Constants::get().int_const(v);
}

bool AlgebraicSimplify::run(PassManager *mgr) {
    bool changed = false;
    for (auto &func_r : mgr->get_module()->functions()) {
        if (func_r.is_external)
            continue;
        ignores.clear();
        for (auto &bb_r : func_r.bbs()) {
            bb = &bb_r;
            auto &insts = bb_r.insts();
            bool bb_iterative_run;
            do {
                bb_iterative_run = false;
                for (auto inst_iter = insts.begin();
                     inst_iter != insts.end();) {
                    inst = &*inst_iter;
                    ++inst_iter;
                    if (contains(ignores, inst))
                        continue;
                    if (apply_rules()) {
                        bb_iterative_run = true;
                        changed = true;
                        ignores.insert(inst);
                    }
                }
            } while (bb_iterative_run);
        }
    }
    return changed;
}

bool AlgebraicSimplify::apply_rules() {
    Value *v1, *v2, *v3, *v4;
    int c1, c2;
    bool i64 = inst->get_type()->is<I64IntType>();

    /* meaningless computation */
    // a + 0 -> a
    if (iadd(any_val(v1), is_cint(0))->match(inst)) {
        inst->replace_all_use_with(v1);
        return true;
    }
    // a - 0 -> a
    if (isub(any_val(v1), is_cint(0))->match(inst)) {
        inst->replace_all_use_with(v1);
        return true;
    }
    // a * 0 -> 0
    if (imul(any_val(v1), is_cint(0))->match(inst)) {
        inst->replace_all_use_with(get_cint(0, i64));
        return true;
    }
    // a * 1 -> a
    if (imul(any_val(v1), is_cint(1))->match(inst)) {
        inst->replace_all_use_with(v1);
        return true;
    }
    // 0 / a -> 0
    if (idiv(is_cint(0), any_val(v1))->match(inst)) {
        inst->replace_all_use_with(get_cint(0, i64));
        return true;
    }
    // a / 1 -> a
    if (idiv(any_val(v1), is_cint(1))->match(inst)) {
        inst->replace_all_use_with(v1);
        return true;
    }

    /* continuous opration on const */
    // (v1 + c1) + c2 -> v1 + (c1 + c2)
    if (iadd(iadd(any_val(v1), is_cint_like(c1)), is_cint_like(c2))
            ->match(inst)) {
        auto add = insert_ibin(IBinaryInst::ADD, v1, get_cint(c1 + c2, i64));
        inst->replace_all_use_with(add);
        return true;
    }
    // (v1 - c1) - c2 -> v1 - (c1 + c2)
    if (isub(isub(any_val(v1), is_cint_like(c1)), is_cint_like(c2))
            ->match(inst)) {
        auto sub = insert_ibin(IBinaryInst::SUB, v1, get_cint(c1 + c2, i64));
        inst->replace_all_use_with(sub);
        return true;
    }
    // (a * c1) * c2 -> a * (c1 * c2)
    if (imul(imul(any_val(v1), is_cint_like(c1)), is_cint_like(c2))
            ->match(inst)) {
        auto mul = insert_ibin(IBinaryInst::MUL, v1, get_cint(c1 * c2, i64));
        inst->replace_all_use_with(mul);
        return true;
    }
    // (v1 / c1) / c2 -> v1 / (c1 * c2)
    if (idiv(idiv(any_val(v1), is_cint_like(c1)), is_cint_like(c2))
            ->match(inst)) {
        assert(c1 != 0 and c2 != 0);
        if (c1 * c2 != static_cast<int64_t>(c1) * c2) { // overflow on i32
            inst->replace_all_use_with(get_cint(0, i64));
        } else {
            auto div =
                insert_ibin(IBinaryInst::SDIV, v1, get_cint(c1 * c2, i64));
            inst->replace_all_use_with(div);
        }
        return true;
    }

    /* counterpart operation */
    // (v1 + v2) - v2 or (v1 - v2) + v2
    // (v1 * v2) / v2 or (v1 / v2) * v2
    if ((isub(iadd(any_val(v1), any_val(v2)), any_val(v3))->match(inst) or
         iadd(isub(any_val(v1), any_val(v2)), any_val(v3))->match(inst) or
         idiv(imul(any_val(v1), any_val(v2)), any_val(v3))->match(inst) or
         imul(idiv(any_val(v1), any_val(v2)), any_val(v3))->match(inst)) and
        v2 == v3) {
        inst->replace_all_use_with(v1);
        return true;
    }
    // (v1 + c1) - c2
    if (isub(iadd(any_val(v1), is_cint_like(c1)), is_cint_like(c2))
            ->match(inst)) {
        auto add = insert_ibin(IBinaryInst::ADD, v1, get_cint(c1 - c2, i64));
        inst->replace_all_use_with(add);
        return true;
    }
    // (v1 - c1) + c2
    if (iadd(isub(any_val(v1), is_cint_like(c1)), is_cint_like(c2))
            ->match(inst)) {
        auto add = insert_ibin(IBinaryInst::ADD, v1, get_cint(c2 - c1, i64));
        inst->replace_all_use_with(add);
        return true;
    }
    // (v1 * c1) / c2 (divide envenly)
    if (idiv(imul(any_val(v1), is_cint_like(c1)), is_cint_like(c2))
            ->match(inst) and
        c1 % c2 == 0) {
        auto mul = insert_ibin(IBinaryInst::MUL, v1, get_cint(c1 / c2, i64));
        inst->replace_all_use_with(mul);
        return true;
    }
    // (v1 / c1) * c2 (divide envenly)
    if (imul(idiv(any_val(v1), is_cint_like(c1)), is_cint_like(c2))
            ->match(inst) and
        c2 % c1 == 0) {
        auto mul = insert_ibin(IBinaryInst::MUL, v1, get_cint(c2 / c1, i64));
        inst->replace_all_use_with(mul);
        return true;
    }

    /* combining */
    // (v1 + v2) + v3
    if (iadd(one_use(iadd(any_val(v1), any_val(v2))), any_val(v3))
            ->match(inst) and
        (v1 == v2 or v1 == v3 or v2 == v3)) {
        // v1 + v1 + v1 -> v1 * 3
        if (v1 == v2 and v2 == v3) {
            auto mul = insert_ibin(IBinaryInst::MUL, v1, get_cint(3, i64));
            inst->replace_all_use_with(mul);
        } else {
            // v1 + v2 + v2 -> v1 + v2 * 2
            if (v1 == v3)
                std::swap(v1, v2);
            else if (v1 == v2)
                std::swap(v1, v3);
            auto mul = insert_ibin(IBinaryInst::MUL, v2, get_cint(2, i64));
            auto add = insert_ibin(IBinaryInst::ADD, v1, mul);
            inst->replace_all_use_with(add);
        }
        return true;
    }
    // v1 * v2 + v3
    // v1 * v2 + v1 -> v1 * (v2 + 1)
    if (iadd(one_use(imul(any_val(v1), any_val(v2))), any_val(v3))
            ->match(inst) and
        (v1 == v3 or v2 == v3)) {
        if (v2 == v3)
            std::swap(v1, v2);
        auto times = insert_ibin(IBinaryInst::ADD, v2, get_cint(1, i64));
        auto mul = insert_ibin(IBinaryInst::MUL, v1, times);
        inst->replace_all_use_with(mul);
        return true;
    }
    // (v1 + (v2 * v3)) + v4
    // (v1 + (v2 * v3)) + v2 -> v1 + v2 * (v3 + 1)
    if (iadd(iadd(any_val(v1), one_use(imul(any_val(v2), any_val(v3)))),
             any_val(v4))
            ->match(inst) and
        (v2 == v4 or v3 == v4)) {

        if (v3 == v4)
            std::swap(v2, v3);
        auto times = insert_ibin(IBinaryInst::ADD, v3, get_cint(1, i64));
        auto mul = insert_ibin(IBinaryInst::MUL, v2, times);
        auto new_v = insert_ibin(IBinaryInst::ADD, v1, mul);
        inst->replace_all_use_with(new_v);
        return true;
    }

    /* strength reduce */ // FIXME overflow bug
    // (v1 * v2) + (v3 * v4)
    // (v1 * v2) + (v3 * v2) -> (v1 + v3) * v2
    if (iadd(one_use(imul(any_val(v1), any_val(v2))),
             one_use(imul(any_val(v3), any_val(v4))))
            ->match(inst) and
        (v1 == v3 or v1 == v4 or v2 == v3 or v2 == v4)) {
        if (v1 == v3) {
            std::swap(v1, v2);
            std::swap(v3, v4);
        } else if (v1 == v4)
            std::swap(v1, v2);
        else if (v2 == v3)
            std::swap(v3, v4);

        assert(v2 == v4);
        auto add = insert_ibin(IBinaryInst::ADD, v1, v3);
        auto div = insert_ibin(IBinaryInst::MUL, add, v2);
        inst->replace_all_use_with(div);
        return true;
    }
    // (v1 / v2) + (v3 / v4)
    // (v1 / v2) + (v3 / v2) -> (v1 + v3) / v2
    if (iadd(one_use(idiv(any_val(v1), any_val(v2))),
             one_use(idiv(any_val(v3), any_val(v4))))
            ->match(inst) and
        v2 == v4) {
        auto add = insert_ibin(IBinaryInst::ADD, v1, v3);
        auto div = insert_ibin(IBinaryInst::SDIV, add, v2);
        inst->replace_all_use_with(div);
        return true;
    }

    return false;
}
