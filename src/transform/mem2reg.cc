#include "mem2reg.hh"
#include "basic_block.hh"
#include "constant.hh"
#include "dominator.hh"
#include "global_variable.hh"
#include "instruction.hh"
#include "usedef_chain.hh"
#include "utils.hh"
#include "value.hh"
#include <cassert>
#include <stdexcept>

using namespace pass;
using namespace ir;
using namespace std;

void Mem2reg::run(PassManager *mgr) {
    _dominator = &mgr->get_result<Dominator>();
    _usedef_chain = &mgr->get_result<UseDefChain>();
    auto m = mgr->get_module();
    for (auto &f : m->functions()) {
        if (f.bbs().size() >= 1) {
            generate_phi(&f);
            re_name(f.get_entry_bb());
        }
        // to remove_alloca? dead_code_elmination can do it, i think
    }
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
            if (is_a<StoreInst>(inst)) {
                if (!is_a<GlobalVariable>(inst->operands()[1]) &&
                    !is_a<GetElementPtrInst>(inst->operands()[1])) {
                    var_kill.insert(inst->operands()[1]);
                    blocks[inst->operands()[1]].insert(bb);
                }
            }
            if (is_a<LoadInst>(inst) &&
                var_kill.find(inst->operands()[0]) == var_kill.end() &&
                !is_a<GlobalVariable>(inst->operands()[0]) &&
                !is_a<GetElementPtrInst>(inst->operands()[0])) {
                globals.insert(inst->operands()[0]);
            }
        }
    }
    // record which block needs a phi inst for var
    for (auto var : globals) {
        vector<BasicBlock *> work_list(blocks[var].begin(), blocks[var].end());
        for (unsigned i = 0; i < work_list.size(); i++) {
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
void Mem2reg::re_name(BasicBlock *bb) {
    map<BasicBlock *, set<Instruction *>> wait_delete{};
    // for each phi in bb _var_new_name[var].push_back(phi)
    for (auto &inst_r : bb->insts()) {
        if (is_a<PhiInst>(&inst_r)) {
            auto inst = as_a<PhiInst>(&inst_r);
            _var_new_name[_phi_lval[inst]].push_back(inst);
        }
    }
    for (auto &inst_r : bb->insts()) {
        auto inst = &inst_r;
        // for loadinst, replace all operands using loadinst with the newest
        // value
        if (is_a<LoadInst>(inst)) {
            auto l_val = inst->operands()[0];
            if (!is_a<GlobalVariable>(l_val) &&
                !is_a<GetElementPtrInst>(l_val)) {
                if (_var_new_name[l_val].size()) {
                    _usedef_chain->replace_all_use_with(
                        inst, _var_new_name[l_val].back());
                } else { // the inst loads an undef value
                    _usedef_chain->replace_all_use_with(
                        inst, Constants::get().undef(inst));
                }
                wait_delete[bb].insert(inst);
            }
        }
        // for storeinst, update the newest value of the allocated var
        else if (is_a<StoreInst>(inst)) {
            auto val = inst->operands()[0];
            auto l_val = inst->operands()[1];
            if (!is_a<GlobalVariable>(l_val) &&
                !is_a<GetElementPtrInst>(l_val) &&
                val->get_type()->is_basic_type()) {
                _var_new_name[inst->operands()[1]].push_back(
                    inst->operands()[0]);
                wait_delete[bb].insert(inst);
            }
        }
    }
    // fill parameters in phi
    for (auto suc : bb->suc_bbs()) {
        for (auto &inst_r : suc->insts()) {
            if (is_a<PhiInst>(&inst_r)) {
                auto inst = as_a<PhiInst>(&inst_r);
                if (_var_new_name[_phi_lval[inst]].size()) {
                    inst->add_phi_param(_var_new_name[_phi_lval[inst]].back(),
                                        bb);
                } else {
                    inst->add_phi_param(Constants::get().undef(inst), bb);
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
            if (!is_a<GlobalVariable>(l_val) &&
                !is_a<GetElementPtrInst>(l_val)) {
                _var_new_name[l_val].pop_back();
            }
        } else if (is_a<PhiInst>(inst)) {
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
            bb->insts().erase(inst);
        }
    }
}