#include "loop_invariant.hh"
#include "basic_block.hh"
#include "constant.hh"
#include "function.hh"
#include "global_variable.hh"
#include "instruction.hh"
#include "log.hh"
#include "loop_find.hh"
#include "type.hh"
#include "utils.hh"
#include <algorithm>
#include <tuple>
#include <vector>

using namespace pass;
using namespace ir;
using namespace std;

using LoopInfo = LoopFind::ResultType::LoopInfo;
using FuncLoopInfo = LoopFind::ResultType::FuncLoopInfo;

bool is_invariant_operand(Value *op, const LoopInfo &loop) {
    if (not op->is<Instruction>()) {
        return true;
    }
    auto inst = as_a<Instruction>(op);
    return not contains(loop.bbs, inst->get_parent());
}

bool is_side_effect_inst(Instruction *inst) {
    return inst->is<LoadInst>() || inst->is<StoreInst>() ||
           inst->is<CallInst>() || inst->is<RetInst>() || inst->is<BrInst>() ||
           inst->is<PhiInst>();
}

vector<Instruction *> collect_invariant_inst(BasicBlock *bb,
                                             const LoopInfo &loop) {
    vector<Instruction *> ret;
    for (auto &&inst : bb->insts()) {
        if (is_side_effect_inst(&inst)) {
            continue;
        }
        bool invariant{true};
        for (auto &&op : inst.operands()) {
            if (not is_invariant_operand(op, loop)) {
                invariant = false;
                break;
            }
        }
        if (invariant) {
            ret.push_back(&inst);
        }
    }
    return ret;
}

using Pair = PhiInst::Pair;

pair<vector<Pair>, vector<Pair>> split_phi_op(PhiInst *phi,
                                              const LoopInfo &loop) {
    vector<Pair> inner, outer;
    for (auto &&[op, bb] : phi->to_pairs()) {
        if (contains(loop.bbs, bb)) {
            inner.emplace_back(op, bb);
        } else {
            outer.emplace_back(op, bb);
        }
    }
    return {inner, outer};
}

BasicBlock *create_preheader(BasicBlock *header, const LoopInfo &loop) {
    // assume all prebbs of loop body are within the loop itself
    // so here we only process prebb of header bb
    auto func = header->get_func();
    auto preheader = func->create_bb();
    for (auto in_bb : header->pre_bbs()) {
        if (not contains(loop.latches, in_bb)) {
            // pre_bb of preheader
            preheader->pre_bbs().push_back(in_bb);
            // suc_bb of in_bb
            auto br = &in_bb->insts().back();

            /* replace(br->operands().begin(), br->operands().end(), header,
             *         preheader); */
            header->replace_all_use_with_if(
                preheader, [&](const Use &use) { return use.user == br; });

            replace(in_bb->suc_bbs().begin(), in_bb->suc_bbs().end(), header,
                    preheader);
        }
    }
    // pre_bb of header
    header->pre_bbs() = loop.latches;
    // we insert branch inst later, which modifies pre_bb of header and suc_bb
    // of preheader too

    // phi
    for (auto &&inst : header->insts()) {
        if (not inst.is<PhiInst>()) {
            break;
        }
        auto phi = inst.as<PhiInst>();
        auto [inner, outer] = split_phi_op(phi, loop);
        if (outer.size() == 0) {
            // all defs are from inside, do not modify
            continue;
        }
        if (inner.size() == 0) {
            // all defs are from outside, move to preheader
            preheader->move_inst(preheader->insts().end(), phi);
            continue;
        }
        if (outer.size() == 1) {
            // only one defs from outside, set source to preheader
            inner.emplace_back(outer[0].first, preheader);
            phi->from_pairs(inner);
            continue;
        }
        // create a new phi in preheader
        auto phi_outer =
            preheader->clone_inst(preheader->insts().end(), phi)->as<PhiInst>();
        phi_outer->from_pairs(outer);
        inner.emplace_back(phi_outer, preheader);
        phi->from_pairs(inner);
    }
    return preheader;
}

void handle_func(Function *func, const FuncLoopInfo &loops) {
    for (auto &&[header, loop] : loops) {
        auto preheader = create_preheader(header, loop);
        bool changed{true};
        while (changed) {
            vector<Instruction *> insts;
            for (auto bb : loop.bbs) {
                auto bb_insts = collect_invariant_inst(bb, loop);
                insts.insert(insts.end(), bb_insts.begin(), bb_insts.end());
            }
            changed = insts.size() > 0;
            for (auto inst : insts) {
                preheader->move_inst(preheader->insts().end(), inst);
            }
        }
        preheader->create_inst<BrInst>(header);

        /* debugs << "invariant of loop " << header->get_name();
         * for (auto &&inst : preheader->insts()) {
         *     debugs << ' ' << inst.get_name();
         * }
         * debugs << '\n'; */
    }
}

void LoopInvariant::run(PassManager *mgr) {
    auto &&loop_info = mgr->get_result<LoopFind>().loop_info;
    auto m = mgr->get_module();
    for (auto &&func : m->functions()) {
        if (func.is_external) {
            continue;
        }
        handle_func(&func, loop_info.at(&func));
    }
}
