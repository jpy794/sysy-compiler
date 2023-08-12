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

using namespace pass;
using namespace ir;
using namespace Matcher;

auto get_cint(int v) {
    return v ? static_cast<Constant *>(Constants::get().int_const(v))
             : Constants::get().zero_const(Types::get().int_type());
}

void AlgebraicSimplify::run(PassManager *mgr) {
    for (auto &func_r : mgr->get_module()->functions()) {
        if (func_r.is_external)
            continue;

        bool changed;
        ignores.clear();
        do {
            changed = false;
            for (auto &bb_r : func_r.bbs()) {
                bb = &bb_r;
                auto &insts = bb_r.insts();
                for (auto inst_iter = insts.begin();
                     inst_iter != insts.end();) {
                    inst = &*inst_iter;
                    ++inst_iter;
                    if (contains(ignores, inst))
                        continue;
                    if (apply_rules()) {
                        changed = true;
                        ignores.insert(inst);
                    }
                }
            }
            if (changed)
                mgr->run({PassID<ConstPro>(), PassID<DeadCode>()}, false);
        } while (changed);
    }
}

bool AlgebraicSimplify::apply_rules() {
    Value *v1, *v2, *v3, *v4;
    int c1, c2;

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
        inst->replace_all_use_with(get_cint(0));
        return true;
    }
    // a * 1 -> a
    if (imul(any_val(v1), is_cint(1))->match(inst)) {
        inst->replace_all_use_with(v1);
        return true;
    }

    // 0 / a -> 0
    if (idiv(is_cint(0), any_val(v1))->match(inst)) {
        inst->replace_all_use_with(get_cint(0));
        return true;
    }
    // a / 1 -> a
    if (idiv(any_val(v1), is_cint(1))->match(inst)) {
        inst->replace_all_use_with(v1);
        return true;
    }

    // v1 + v2 + v3
    if (iadd(one_use(iadd(any_val(v1), any_val(v2))), any_val(v3))
            ->match(inst) and
        (v1 == v2 or v1 == v3 or v2 == v3)) {
        // v1 + v1 + v1 -> v1 * 3
        if (v1 == v2 and v2 == v3) {
            auto mul = insert_ibin(IBinaryInst::MUL, v1, get_cint(3));
            inst->replace_all_use_with(mul);
        } else {
            // v1 + v2 + v2 -> v1 + v2 * 2
            if (v1 == v3)
                std::swap(v1, v2);
            else if (v1 == v2)
                std::swap(v1, v3);
            auto mul = insert_ibin(IBinaryInst::MUL, v2, get_cint(2));
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
        auto times = insert_ibin(IBinaryInst::ADD, v2, get_cint(1));
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
        auto times = insert_ibin(IBinaryInst::ADD, v3, get_cint(1));
        auto mul = insert_ibin(IBinaryInst::MUL, v2, times);
        auto new_v = insert_ibin(IBinaryInst::ADD, v1, mul);
        inst->replace_all_use_with(new_v);
        return true;
    }

    // (a * c1) * c2 -> a * (c1 * c2)
    if (imul(imul(any_val(v1), is_cint_like(c1)), is_cint_like(c2))
            ->match(inst)) {
        auto const_int = c1 * c2;
        auto mul = insert_ibin(IBinaryInst::MUL, v1, get_cint(const_int));
        inst->replace_all_use_with(mul);
        return true;
    }

    // a * c1 / c2 -> a * (c1/c2) (divided evenly)
    if (idiv(imul(any_val(v1), is_cint_like(c1)), is_cint_like(c2))
            ->match(inst) and
        (c1 % c2 == 0)) {
        auto mul = c1 / c2;
        if (mul == 1)
            inst->replace_all_use_with(v1);
        else {
            auto val = insert_ibin(IBinaryInst::MUL, v1, get_cint(mul));
            inst->replace_all_use_with(val);
        }
        return true;
    }

    return false;
}
