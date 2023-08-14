#include "mem2reg.hh"
#include "basic_block.hh"
#include "constant.hh"
#include "dominator.hh"
#include "err.hh"
#include "global_variable.hh"
#include "instruction.hh"
#include "type.hh"
#include "usedef_chain.hh"
#include "utils.hh"
#include "value.hh"
#include <cassert>
#include <stdexcept>

using namespace pass;
using namespace ir;
using namespace std;

bool pointer_is_basic_alloc(Value *v) {
    assert(v->get_type()->is<PointerType>());
    if (is_a<GlobalVariable>(v) or is_a<GetElementPtrInst>(v))
        return false;
    else if (is_a<AllocaInst>(v))
        return v->get_type()
            ->as<PointerType>()
            ->get_elem_type()
            ->is_basic_type();
    else
        throw unreachable_error{};
}

void Mem2reg::clear() {
    _phi_table.clear();
    _phi_lval.clear();
    _var_new_name.clear();
}

bool Mem2reg::run(PassManager *mgr) {
    clear();
    _dominator = &mgr->get_result<Dominator>();
    auto m = mgr->get_module();
    for (auto &f : m->functions()) {
        if (f.bbs().size() >= 1) {
            generate_phi(&f);
            re_name(f.get_entry_bb());
        }
    }
    return false;
}

void Mem2reg::generate_phi(Function *f) {
    set<Value *> globals{};
    map<Value *, set<BasicBlock *>> blocks{};
    // get the set of global name and the probably active block of them
    for (auto &bb_r : f->bbs()) {
        auto bb = &bb_r;
        set<Value *> var_kill{};
        for (auto &inst_r : bb->insts()) {
            auto inst = &inst_r;
            if (is_a<StoreInst>(inst) and
                pointer_is_basic_alloc(inst->operands()[1])) {
                var_kill.insert(inst->operands()[1]);
                blocks[inst->operands()[1]].insert(bb);
            }
            if (is_a<LoadInst>(inst) and
                not contains(var_kill, inst->operands()[0]) and
                pointer_is_basic_alloc(inst->operands()[0])) {
                globals.insert(inst->operands()[0]);
            }
        }
    }
    // record which block needs a phi inst for var
    for (auto var : globals) {
        vector<BasicBlock *> work_list(blocks[var].begin(), blocks[var].end());
        for (unsigned i = 0; i < work_list.size(); ++i) {
            auto bb = work_list[i];
            for (auto df_bb : _dominator->dom_frontier.at(bb)) {
                if (_phi_table.find({var, df_bb}) == _phi_table.end()) {
                    auto phi = df_bb->insert_inst<PhiInst>(
                        df_bb->insts().begin(), var);
                    work_list.push_back(df_bb);
                    _phi_lval[phi] = var;
                    _phi_table.insert({var, df_bb});
                }
            }
        }
    }
}

bool Mem2reg::is_wanted_phi(Value *v) {
    return is_a<PhiInst>(v) and contains(_phi_lval, as_a<PhiInst>(v));
}

void Mem2reg::re_name(BasicBlock *bb) {
    map<BasicBlock *, set<Instruction *>> wait_delete{};
    // for each phi in bb _var_new_name[var].push_back(phi)
    for (auto &inst_r : bb->insts()) {
        if (is_wanted_phi(&inst_r)) {
            auto phi = as_a<PhiInst>(&inst_r);
            _var_new_name[_phi_lval.at(phi)].push_back(phi);
        }
    }
    for (auto &inst_r : bb->insts()) {
        auto inst = &inst_r;
        // for loadinst, replace all operands using loadinst with the newest
        // value
        if (is_a<LoadInst>(inst)) {
            auto l_val = inst->operands()[0];
            if (pointer_is_basic_alloc(l_val)) {
                if (_var_new_name[l_val].size()) {
                    inst->replace_all_use_with(_var_new_name[l_val].back());
                } else { // the inst loads an undef value
                    inst->replace_all_use_with(Constants::get().undef(inst));
                }
                wait_delete[bb].insert(inst);
            }
        }
        // for storeinst, update the newest value of the allocated var
        else if (is_a<StoreInst>(inst)) {
            // auto val = inst->operands()[0];
            auto l_val = inst->operands()[1];
            if (pointer_is_basic_alloc(l_val)) {
                _var_new_name[l_val].push_back(inst->operands()[0]);
                wait_delete[bb].insert(inst);
            }
        }
    }
    // fill parameters in phi
    for (auto suc : bb->suc_bbs()) {
        for (auto &inst_r : suc->insts()) {
            if (is_wanted_phi(&inst_r)) {
                auto phi = as_a<PhiInst>(&inst_r);
                if (_var_new_name[_phi_lval[phi]].size()) {
                    phi->add_phi_param(_var_new_name[_phi_lval[phi]].back(),
                                       bb);
                } else {
                    phi->add_phi_param(Constants::get().undef(phi), bb);
                }
            }
        }
    }
    // re_name all the dom_succ_bb of current bb
    for (auto dom_succ_bb : _dominator->dom_tree_succ_blocks.at(bb)) {
        re_name(dom_succ_bb);
    }

    // pop the newest value of l_val
    for (auto &inst_r : bb->insts()) {
        auto inst = &inst_r;

        if (is_a<StoreInst>(inst)) {
            auto l_val = inst->operands()[1];
            if (pointer_is_basic_alloc(l_val)) {
                _var_new_name[l_val].pop_back();
            }
        } else if (is_wanted_phi(inst)) {
            auto l_val = _phi_lval[as_a<PhiInst>(inst)];
            if (_var_new_name.find(l_val) != _var_new_name.end()) {
                _var_new_name[l_val].pop_back();
            } else {
                throw logic_error{
                    "In mem2reg, the newest value of phi pop error"};
            }
        }
    }
    // delete redundant store/load instructions produced by mem2reg
    for (auto bb_inst_pair : wait_delete) {
        auto bb = bb_inst_pair.first;
        for (auto inst : bb_inst_pair.second) {
            bb->erase_inst(inst);
        }
    }
}
