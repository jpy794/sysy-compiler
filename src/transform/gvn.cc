#include "gvn.hh"
#include "basic_block.hh"
#include "constant.hh"
#include "depth_order.hh"
#include "dominator.hh"
#include "func_info.hh"
#include "function.hh"
#include "instruction.hh"
#include "log.hh"
#include "type.hh"
#include "usedef_chain.hh"
#include "utils.hh"
#include "value.hh"
#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

using namespace pass;
using namespace ir;
using namespace std;

namespace std {
template <>
bool operator==(const shared_ptr<pass::GVN::Expression> &lhs,
                const shared_ptr<pass::GVN::Expression> &rhs) noexcept {
    return *lhs == *rhs;
}
} // namespace std

bool operator==(const GVN::partitions &lhs, const GVN::partitions &rhs) {
    if (lhs.size() != rhs.size())
        return false;
    else {
        GVN::partitions::iterator i = lhs.begin();
        GVN::partitions::iterator j = rhs.begin();
        for (; i != lhs.end(); i++, j++) {
            if (**i == **j)
                continue;
            else if (**i == **j)
                return false;
            else
                return false;
        }
    }
    return true;
}

bool GVN::CongruenceClass::operator==(const CongruenceClass &other) const {
    if (this->members.size() != other.members.size())
        return false;
    if (!(this->leader == other.leader))
        return false;
    for (auto &mem : other.members) {
        if (contains(this->members, mem))
            continue;
        else
            return false;
    }
    return true;
}

void GVN::run(PassManager *mgr) {
    _func_info = &mgr->get_result<FuncInfo>();
    _usedef_chain = &mgr->get_result<UseDefChain>();
    _depth_order = &mgr->get_result<DepthOrder>();

    auto m = mgr->get_module();
    for (auto &f : m->functions()) {
        if (f.is_external)
            continue;
        _func = &f;
        // initialize the global variables
        {
            next_value_number = 1;
            _pin.clear();
            _pout.clear();
        }
        detect_equivalences(&f);
        replace_cc_members();
    }
}

GVN::partitions GVN::join(const partitions &p1, const partitions &p2) {
    if (p1 == TOP)
        return clone(p2);
    if (p2 == TOP)
        return clone(p1);
    partitions p{};
    for (auto &Ci : p1) {
        for (auto &Cj : p2) {
            auto Ck = intersect(Ci, Cj);
            if (!Ck->members.empty()) {
                p.insert(Ck);
            }
        }
    }
    return p;
}

shared_ptr<GVN::CongruenceClass>
GVN::intersect(shared_ptr<CongruenceClass> Ci, shared_ptr<CongruenceClass> Cj) {
    auto Ck = create_cc(0);
    if (Ci->index == Cj->index)
        Ck->index = Ci->index;
    if (Ci->leader == Cj->leader)
        Ck->leader = Ci->leader;
    if (Ci->val_expr == Cj->val_expr)
        Ck->val_expr = Ci->val_expr;
    if (Ci->phi_expr == Cj->phi_expr)
        Ck->phi_expr = Ci->phi_expr;
    set_intersection(Ci->members.begin(), Ci->members.end(),
                     Cj->members.begin(), Cj->members.end(),
                     inserter(Ck->members, Ck->members.begin()));
    if (!Ck->members.empty() and
        Ck->index == 0) { // FIXME: more than two predecessor blocks?
        Ck->index = next_value_number++;
        Ck->leader = *Ck->members.begin();
        if (Ci->val_expr == Cj->val_expr)
            Ck->val_expr = Ci->val_expr;
        else {
            Ck->phi_expr = create_expr<PhiExpr>(
                vector<shared_ptr<Expression>>{Ci->val_expr, Cj->val_expr},
                vector<ir::BasicBlock *>{
                    nullptr, nullptr}); // FIXME:the bbs need to be filled
            Ck->val_expr = Ck->phi_expr;
        }
    }
    return Ck;
}

void GVN::detect_equivalences(Function *func) {
    // initialize POUT of each bb
    for (auto &bb : func->bbs()) {
        _pout[&bb] = TOP;
    }
    for (auto arg : func->get_args()) {
        auto cc = create_cc(next_value_number++, arg,
                            create_expr<UniqueExpr>(arg), nullptr, arg);
        _pin[func->get_entry_bb()].insert(cc);
    }
    bool changed = false;
    unsigned times = 0;
    do {
        changed = false;
        debugs << "iter" << times << ":\n";
        for (auto &bb : _depth_order->_depth_priority_order.at(_func)) {
            auto &pre_bbs = bb->pre_bbs();
            partitions origin_pout = _pout[bb];
            partitions pin = TOP;
            for (auto pre_bb : pre_bbs)
                pin = join(pin, _pout[pre_bb]);
            if (bb == func->get_entry_bb()) {
                _pout[bb] = clone(_pin[bb]);
            } else {
                _pin[bb] = clone(pin);
                _pout[bb] = clone(pin);
            }
            for (auto &inst_r : bb->insts()) {
                if (::is_a<BrInst>(&inst_r) || ::is_a<PhiInst>(&inst_r) ||
                    ::is_a<StoreInst>(&inst_r) || ::is_a<RetInst>(&inst_r) ||
                    (::is_a<CallInst>(&inst_r) &&
                     _usedef_chain->users.at(&inst_r)
                         .empty())) // For pure_func, if it isn't used, it
                                    // will be optimized by mem2reg
                                    // Thus, this case must be a non-pure_func
                                    // and if it isn't be used, it will not be
                                    // as a basic ValueExpression
                    continue;
                _pout[bb] = transfer_function(&inst_r, _pout[bb]);
            }
            for (auto suc_bb : bb->suc_bbs()) {
                if (_pout[bb] == TOP)
                    break;
                for (auto &inst_r : suc_bb->insts()) {
                    if (::is_a<PhiInst>(&inst_r)) {
                        auto inst = ::as_a<PhiInst>(&inst_r);
                        // deal with the case that the origin of phi is also the
                        // phi of current bb
                        // op1 = phi opi, op1
                        for (auto &Ci : _pout[bb]) {
                            if (contains(Ci->members,
                                         static_cast<Value *>(inst))) {
                                Ci->members.erase(inst);
                                if (Ci->members.size() == 0)
                                    _pout[bb].erase(Ci);
                                break;
                            }
                        }
                        unsigned i = 1;
                        for (; i < inst->operands().size(); i += 2) {
                            if (inst->operands()[i] == bb)
                                break;
                        }
                        assert(i < inst->operands().size());
                        auto oper = inst->get_operand(i - 1);
                        bool flag = true;
                        for (auto &CC : _pout[bb]) {
                            if (contains(CC->members, oper)) {
                                CC->members.insert(inst);
                                flag = false;
                                break;
                            }
                        }
                        if (flag) { // create temporary CC for phi
                            shared_ptr<Expression> oper_expr;
                            if (::is_a<Constant>(oper))
                                oper_expr = create_expr<ConstExpr>(oper);
                            else
                                oper_expr = create_expr<UniqueExpr>(oper);
                            auto cc = create_cc(next_value_number++, oper,
                                                oper_expr, nullptr, oper);
                            cc->members.insert(inst);
                            _pout[bb].insert(cc);
                        }
                    } else
                        break;
                }
            }
            debugs << bb->get_name() << ":{\n";
            if (_pout[bb] == TOP)
                debugs << "TOP";
            else
                for (auto cc : _pout[bb]) {
                    debugs << "v" << cc->index << ":";
                    debugs << cc->val_expr->print() << "[";
                    for (auto mem : cc->members) {
                        debugs << mem->get_name() << " ";
                    }
                    debugs << "]\n";
                }
            debugs << "}\n";
            if (not(origin_pout == _pout[bb])) {
                changed = true;
                if (origin_pout == TOP)
                    continue;
                debugs << "diff:\n\n";
                debugs << "last time\n\n";
                for (auto cc : origin_pout) {
                    debugs << "v" << cc->index << ":";
                    debugs << cc->val_expr->print() << "[";
                    for (auto mem : cc->members) {
                        debugs << mem->get_name() << " ";
                    }
                    debugs << "]\n";
                }
                debugs << "\n\n";
            }
        }
        assert(times++ < 10);
    } while (changed);
}

GVN::partitions GVN::transfer_function(Instruction *inst, partitions &pin) {
    partitions pout = clone(pin);
    for (auto cc : pout) {
        if (contains(cc->members, static_cast<Value *>(inst))) {
            cc->members.erase(inst);
        }
    }
    auto ve = valueExpr(inst, pin);
    auto vpf = valuePhiFunc(ve, pin);
    bool exist_cc = false;
    if (pout == TOP)
        pout.clear();
    for (auto cc : pout) {
        if (cc->val_expr == ve || (vpf != nullptr && cc->phi_expr == vpf)) {
            cc->members.insert(inst);
            cc->val_expr = ve;
            exist_cc = true;
            break;
        }
    }
    assert(ve);
    if (not exist_cc) {
        auto cc = create_cc(next_value_number++, inst, ve, vpf, inst);
        pout.insert(cc);
    }
    return pout;
}
shared_ptr<GVN::Expression> GVN::valueExpr(Value *val, partitions &pin) {
    assert(val);
    auto ve = get_ve(val, pin);
    if (ve)
        return ve;
    if (::is_a<Constant>(val)) {
        return create_expr<ConstExpr>(val);
    } else if (::is_a<CallInst>(val)) {
        auto call = ::as_a<CallInst>(val);
        auto func = call->operands()[0]->as<Function>();
        if (_func_info->is_pure_function(func)) {
            vector<shared_ptr<Expression>> params;
            for (unsigned i = 1; i < call->operands().size(); i++) {
                params.push_back(valueExpr(call->operands()[i], pin));
            }
            return create_expr<CallExpr>(func, std::move(params));
        } else {
            return create_expr<CallExpr>(call);
        }
    } else if (::is_a<ZextInst>(val) || ::is_a<Fp2siInst>(val) ||
               ::is_a<Si2fpInst>(val)) {
        auto oper = ::as_a<Instruction>(val)->get_operand(0);
        return create_expr<UnitExpr>(valueExpr(oper, pin));
    } else if (::is_a<IBinaryInst>(val)) {
        auto op = ::as_a<IBinaryInst>(val)->get_ibin_op();
        return create_BinOperExpr<IBinExpr, IBinaryInst>(op, val, pin);
    } else if (::is_a<FBinaryInst>(val)) {
        auto op = ::as_a<FBinaryInst>(val)->get_fbin_op();
        return create_BinOperExpr<FBinExpr, FBinaryInst>(op, val, pin);
    } else if (::is_a<ICmpInst>(val)) {
        auto op = ::as_a<ICmpInst>(val)->get_icmp_op();
        return create_BinOperExpr<ICmpExpr, ICmpInst>(op, val, pin);
    } else if (::is_a<FCmpInst>(val)) {
        auto op = ::as_a<FCmpInst>(val)->get_Fcmp_op();
        return create_BinOperExpr<FCmpExpr, FCmpInst>(op, val, pin);
    } else if (::is_a<GetElementPtrInst>(val)) {
        auto gep = ::as_a<GetElementPtrInst>(val);
        vector<shared_ptr<Expression>> idxs;
        for (unsigned i = 0; i < gep->operands().size(); i++) {
            idxs.push_back(valueExpr(gep->operands()[i], pin));
        }
        return create_expr<GepExpr>(std::move(idxs));
    } else if (::is_a<PhiInst>(val)) {
        // auto inst = ::as_a<Instruction>(val);
        // vector<shared_ptr<Expression>> vals;
        // vector<BasicBlock *> bbs;
        // for (unsigned i = 0; i < inst->operands().size(); i += 2) {
        //     bbs.push_back(::as_a<BasicBlock>(inst->get_operand(i + 1)));
        //     vals.push_back(valueExpr(inst->get_operand(i),
        //     _pout[bbs.back()]));
        // }
        // return create_expr<PhiExpr>(vals, bbs);
        return create_expr<UniqueExpr>(val); // create a temporary Expression;
    } else {
        return create_expr<UniqueExpr>(val);
    }
}
std::shared_ptr<GVN::PhiExpr> GVN::valuePhiFunc(shared_ptr<Expression> ve,
                                                partitions &pin) {
    if (not is_a<BinExpr>(ve))
        return nullptr;
    auto lhs = as_a<BinExpr>(ve)->get_lhs();
    auto rhs = as_a<BinExpr>(ve)->get_rhs();
    if (not is_a<PhiExpr>(lhs) or not is_a<PhiExpr>(rhs))
        return nullptr;
    auto phi_lhs = as_a<PhiExpr>(lhs);
    auto phi_rhs = as_a<PhiExpr>(rhs);
    if (phi_lhs->size() != phi_rhs->size())
        return nullptr;
    auto res_phi = create_expr<PhiExpr>();
    for (unsigned i = 0; i < phi_lhs->size(); i++) {
        if (phi_lhs->get_suc_bb(i) != phi_rhs->get_suc_bb(i))
            return nullptr;
        shared_ptr<Expression> com;
        switch (ve->get_op()) {
        case Expression::expr_type::e_ibin:
            com =
                create_expr<IBinExpr>(as_a<IBinExpr>(ve)->get_ibin_op(),
                                      phi_lhs->get_val(i), phi_rhs->get_val(i));
            break;
        case Expression::expr_type::e_fbin:
            com =
                create_expr<FBinExpr>(as_a<FBinExpr>(ve)->get_fbin_op(),
                                      phi_lhs->get_val(i), phi_rhs->get_val(i));
            break;
        case Expression::expr_type::e_icmp:
            com =
                create_expr<ICmpExpr>(as_a<ICmpExpr>(ve)->get_icmp_op(),
                                      phi_lhs->get_val(i), phi_rhs->get_val(i));
            break;
        case Expression::expr_type::e_fcmp:
            com =
                create_expr<FCmpExpr>(as_a<FCmpExpr>(ve)->get_fcmp_op(),
                                      phi_lhs->get_val(i), phi_rhs->get_val(i));
            break;
        default:
            assert(false);
        }
        auto pout_i = _pout[phi_lhs->get_suc_bb(i)];
        auto vn = getVN(pout_i, com);
        if (vn == nullptr)
            vn = valuePhiFunc(com, pout_i);
        if (vn == nullptr)
            return nullptr;
        else {
            res_phi->add_val_bb(vn, phi_lhs->get_suc_bb(i));
        }
    }
    return res_phi;
}
std::shared_ptr<GVN::Expression> GVN::get_ve(Value *val, partitions &pout) {
    for (auto cc : pout) {
        if (contains(cc->members, val))
            return cc->val_expr;
    }
    return nullptr;
}
std::shared_ptr<GVN::Expression> GVN::getVN(partitions &pin,
                                            shared_ptr<Expression> ve) {
    for (auto &cc : pin) {
        if (cc->val_expr == ve) {
            return cc->val_expr;
        }
    }
    return nullptr;
}
GVN::partitions GVN::clone(const GVN::partitions &p) {
    partitions clone_p;
    for (auto &cc : p) {
        clone_p.insert(std::make_shared<CongruenceClass>(*cc));
    }
    return clone_p;
}

void GVN::replace_cc_members() {
    for (auto &[bb_r, part] : _pout) {
        auto bb = bb_r;
        for (auto &cc : part) {
            if (cc->index == 0)
                continue;
            for (auto &member : cc->members) {
                bool member_is_phi = ::is_a<PhiInst>(member);
                bool value_phi = cc->phi_expr != nullptr;
                if (member != cc->leader and not ::is_a<Constant>(member) and
                    (value_phi or !member_is_phi)) {
                    /*FIXME:
                    bb1:
                    op0 = phi ... v1
                    op1 = phi ... v1
                    bb2 pre_bb:bb1,bb2
                    op2 = phi op1, ...
                    op3 = phi ..., op2
                    it will replace op2 with op0
                    */
                    // only replace the members if users are in the same block
                    // as bb
                    assert(cc->leader);
                    _usedef_chain->replace_use_when(
                        member, cc->leader, [bb](User *user) {
                            if (auto inst = dynamic_cast<Instruction *>(user)) {
                                auto parent = inst->get_parent();
                                auto &bb_pre = parent->pre_bbs();
                                if (::is_a<PhiInst>(inst))
                                    return contains(bb_pre, bb);
                                else
                                    return parent == bb;
                            }
                            return false;
                        });
                }
            }
        }
    }
    return;
}
