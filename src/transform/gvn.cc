#include "gvn.hh"
#include "basic_block.hh"
#include "constant.hh"
#include "depth_order.hh"
#include "dominator.hh"
#include "err.hh"
#include "func_info.hh"
#include "function.hh"
#include "global_variable.hh"
#include "instruction.hh"

#include "type.hh"
#include "utils.hh"
#include "value.hh"
#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace pass;
using namespace ir;
using namespace std;

namespace std {
template <>
bool operator==(const shared_ptr<pass::GVN::Expression> &lhs,
                const shared_ptr<pass::GVN::Expression> &rhs) noexcept {
    if (lhs == nullptr || rhs == nullptr)
        return false;
    return *lhs == *rhs;
}
template <>
bool operator==(const shared_ptr<pass::GVN::PhiExpr> &lhs,
                const shared_ptr<pass::GVN::PhiExpr> &rhs) noexcept {
    if (lhs == nullptr || rhs == nullptr)
        return false;
    return dynamic_pointer_cast<GVN::Expression>(lhs) ==
           dynamic_pointer_cast<GVN::Expression>(rhs);
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

bool GVN::run(PassManager *mgr) {
    _func_info = &mgr->get_result<FuncInfo>();
    _depth_order = &mgr->get_result<DepthOrder>();
    clear();
    auto m = mgr->get_module();
    for (auto &gv : m->global_vars()) {
        _val2expr[&gv] = create_expr<UniqueExpr>(&gv);
    }
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
    return false;
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
    if (Ci->val_expr == Cj->val_expr)
        Ck->val_expr = Ci->val_expr;
    if (Ci->phi_expr == Cj->phi_expr)
        Ck->phi_expr = Ci->phi_expr;
    set_intersection(Ci->members.begin(), Ci->members.end(),
                     Cj->members.begin(), Cj->members.end(),
                     inserter(Ck->members, Ck->members.begin()));
    if (Ci->leader == Cj->leader)
        Ck->leader = Ci->leader;
    else {
        unsigned order_id = 0;
        for (auto mem :
             Ck->members) { // when there is a non-phi inst in members,
                            // the phi must be equal to the inst.
            if (::is_a<Constant>(mem)) {
                Ck->leader = mem;
                break;
            } else if (::is_a<Instruction>(
                           mem)) { // Select the first inst in depth_first_order
                auto bb = ::as_a<Instruction>(mem)->get_parent();
                if (order_id <= _depth_order->_post_order_id.at(_func).at(bb)) {
                    order_id = _depth_order->_post_order_id.at(_func).at(bb);
                    Ck->leader = mem;
                }
            } else { // for globalvar and argument, it shouldn't be intersected
                     // after mem2reg
                throw logic_error{mem->get_type()->print() +
                                  "has been intersected"};
            }
        }
    }
    assert(Ck->leader || Ck->members.empty());
    if (!Ck->members.empty() and Ck->val_expr == nullptr) {
        if (::is_a<PhiInst>(Ck->leader)) {
            Ck->phi_expr = create_expr<PhiExpr>(_bb);
            assert(phi_construct_point);
            if (phi_construct_point == 1) {
                Ck->phi_expr->add_val(Ci->val_expr);
            } else {
                if (not is_a<PhiExpr>(Ci->val_expr) ||
                    as_a<PhiExpr>(Ci->val_expr)->get_ori_bb() != _bb ||
                    as_a<PhiExpr>(Ci->val_expr)->size() >
                        phi_construct_point) { // this case guarantee that
                                               // Ci->val_expr must be the same
                                               // data in previous predecessor
                    for (unsigned i = 0; i < phi_construct_point; i++) {
                        Ck->phi_expr->add_val(Ci->val_expr);
                    }
                } else {
                    Ck->phi_expr = Ci->phi_expr;
                }
            }
            Ck->phi_expr->add_val(Cj->val_expr);
            Ck->val_expr = Ck->phi_expr;
            assert(Ck->phi_expr->size() == (phi_construct_point + 1));
        } else {
            Ck->val_expr = Ci->index > Cj->index ? Ci->val_expr : Cj->val_expr;
        }
    }
    if (Ck->index == 0 && not Ck->members.empty())
        Ck->index = next_value_number++;
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
        for (auto &bb_r : _depth_order->_depth_priority_order.at(_func)) {
            _bb = bb_r;
            auto &pre_bbs = _bb->pre_bbs();
            partitions origin_pout = _pout[_bb];
            partitions pin = TOP;
            phi_construct_point = 0;
            for (auto pre_bb : pre_bbs) {
                pin = join(pin, _pout[pre_bb]);
                phi_construct_point++;
            }
            if (_bb == func->get_entry_bb()) {
                _pout[_bb] = clone(_pin[_bb]);
            } else {
                _pin[_bb] = clone(pin);
                _pout[_bb] = clone(pin);
            }
            for (auto &inst_r : _bb->insts()) {
                if (::is_a<BrInst>(&inst_r) || ::is_a<PhiInst>(&inst_r) ||
                    ::is_a<RetInst>(&inst_r) ||
                    (::is_a<CallInst>(&inst_r) &&
                     inst_r.get_use_list()
                         .empty())) // For pure_func, if it isn't used, it
                                    // will be optimized by mem2reg
                                    // Thus, this case must be a non-pure_func
                                    // and if it isn't be used, it will not be
                                    // as a basic ValueExpression
                    continue;
                _pout[_bb] = transfer_function(&inst_r, _pout[_bb]);
                // special judgement to prevent from large number of CC
                if (_pout[_bb].size() > 200) {
                    non_copy_pout.clear();
                    return;
                }
            }
            non_copy_pout[_bb] = clone(_pout[_bb]);
            for (auto suc_bb : _bb->suc_bbs()) {
                if (_pout[_bb] == TOP)
                    break;
                for (auto &inst_r : suc_bb->insts()) {
                    if (::is_a<PhiInst>(&inst_r)) {
                        auto inst = ::as_a<PhiInst>(&inst_r);
                        unsigned i = 1;
                        for (; i < inst->operands().size(); i += 2) {
                            if (inst->operands()[i] == _bb)
                                break;
                        }
                        assert(i < inst->operands().size());
                        auto oper = inst->get_operand(i - 1);
                        // transfer inst according to oper's CC to prevent from
                        // CC increasing unlimitedly
                        if (inst == oper) // if oper is inst itself, no transfer
                            continue;
                        for (auto &Ci : _pout[_bb]) {
                            if (contains(Ci->members,
                                         static_cast<Value *>(inst))) {
                                Ci->members.erase(inst);
                                if (Ci->members.size() == 0)
                                    _pout[_bb].erase(Ci);
                                break;
                            }
                        }
                        bool flag = true;
                        for (auto &cc : _pout[_bb]) {
                            if (contains(cc->members, oper)) {
                                cc->members.insert(inst);
                                flag = false;
                                break;
                            }
                        }
                        assert(not flag || not ::is_a<Instruction>(oper));
                        if (flag) {
                            assert(::is_a<Constant>(oper) ||
                                   ::is_a<GlobalVariable>(oper));
                            shared_ptr<Expression> ve;
                            ve = valueExpr(oper, _pout[_bb]);
                            auto cc = create_cc(next_value_number++, oper, ve,
                                                nullptr, oper);
                            cc->members.insert(inst);
                            _pout[_bb].insert(cc);
                        }
                    } else
                        break;
                }
            }
            if (not(origin_pout == _pout[_bb])) {
                changed = true;
            }
        }
        assert(times++ < 100);
    } while (changed);
}

GVN::partitions GVN::transfer_function(Instruction *inst, partitions &pin) {
    partitions pout = clone(pin);
    for (auto cc : pout) {
        if (contains(cc->members, static_cast<Value *>(inst))) {
            cc->members.erase(inst);
            if (cc->members.size() == 0)
                pout.erase(cc);
        }
    }
    auto ve = valueExpr(inst, pin);
    auto vpf = valuePhiFunc(ve);
    bool exist_cc = false;
    if (pout == TOP)
        pout.clear();
    for (auto cc : pout) {
        if (cc->val_expr == ve || (vpf != nullptr && cc->phi_expr == vpf)) {
            cc->members.insert(inst);
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
    if (::is_a<Constant>(val)) {
        if (_val2expr[val])
            return _val2expr[val];
        return _val2expr[val] = create_expr<ConstExpr>(val);
    } else if (::is_a<GlobalVariable>(val)) {
        return _val2expr[val];
    }
    std::shared_ptr<Expression> ve = get_ve(val, pin);
    if (ve)
        return ve;
    else if (::is_a<CallInst>(val)) {
        auto call = ::as_a<CallInst>(val);
        auto func = call->operands()[0]->as<Function>();
        if (_func_info->is_pure_function(func)) {
            vector<shared_ptr<Expression>> params;
            for (unsigned i = 1; i < call->operands().size(); i++) {
                params.push_back(valueExpr(call->operands()[i], pin));
            }
            ve = create_expr<CallExpr>(func, std::move(params));
        } else {
            ve = create_expr<CallExpr>(call);
        }
    } else if (::is_a<ZextInst>(val) || ::is_a<Fp2siInst>(val) ||
               ::is_a<Si2fpInst>(val)) {
        auto oper = ::as_a<Instruction>(val)->get_operand(0);
        ve = create_expr<UnitExpr>(valueExpr(oper, pin));
    } else if (::is_a<IBinaryInst>(val)) {
        auto op = ::as_a<IBinaryInst>(val)->get_ibin_op();
        ve = create_BinOperExpr<IBinExpr, IBinaryInst>(op, val, pin);
    } else if (::is_a<FBinaryInst>(val)) {
        auto op = ::as_a<FBinaryInst>(val)->get_fbin_op();
        ve = create_BinOperExpr<FBinExpr, FBinaryInst>(op, val, pin);
    } else if (::is_a<ICmpInst>(val)) {
        auto op = ::as_a<ICmpInst>(val)->get_icmp_op();
        ve = create_BinOperExpr<ICmpExpr, ICmpInst>(op, val, pin);
    } else if (::is_a<FCmpInst>(val)) {
        auto op = ::as_a<FCmpInst>(val)->get_fcmp_op();
        ve = create_BinOperExpr<FCmpExpr, FCmpInst>(op, val, pin);
    } else if (::is_a<GetElementPtrInst>(val)) {
        auto gep = ::as_a<GetElementPtrInst>(val);
        vector<shared_ptr<Expression>> idxs;
        for (unsigned i = 0; i < gep->operands().size(); i++) {
            idxs.push_back(valueExpr(gep->operands()[i], pin));
        }
        ve = create_expr<GepExpr>(std::move(idxs));
    } else if (::is_a<PhiInst>(val)) {
        throw unreachable_error{};
    } else if (::is_a<AllocaInst>(val)) {
        ve = create_expr<UniqueExpr>(val);
    } else if (::is_a<LoadInst>(val)) {
        ve = create_expr<LoadExpr>(
            valueExpr(::as_a<LoadInst>(val)->get_operand(0), pin));
    } else if (::is_a<StoreInst>(val)) {
        ve = create_expr<StoreExpr>(
            valueExpr(::as_a<StoreInst>(val)->get_operand(0), pin),
            valueExpr(::as_a<StoreInst>(val)->get_operand(1), pin));
    } else {
        throw logic_error{"Can't create a temporary expression"};
    }
    assert(ve);
    return ve;
}
std::shared_ptr<GVN::PhiExpr> GVN::valuePhiFunc(shared_ptr<Expression> ve) {
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
    auto bb = phi_lhs->get_ori_bb();
    auto res_phi = create_expr<PhiExpr>(bb);
    for (unsigned i = 0; i < phi_lhs->size(); i++) {
        shared_ptr<Expression> tmp;
        switch (ve->get_op()) {
        case Expression::expr_type::e_ibin:
            tmp =
                create_expr<IBinExpr>(as_a<IBinExpr>(ve)->get_ibin_op(),
                                      phi_lhs->get_val(i), phi_rhs->get_val(i));
            break;
        case Expression::expr_type::e_fbin:
            tmp =
                create_expr<FBinExpr>(as_a<FBinExpr>(ve)->get_fbin_op(),
                                      phi_lhs->get_val(i), phi_rhs->get_val(i));
            break;
        case Expression::expr_type::e_icmp:
            tmp =
                create_expr<ICmpExpr>(as_a<ICmpExpr>(ve)->get_icmp_op(),
                                      phi_lhs->get_val(i), phi_rhs->get_val(i));
            break;
        case Expression::expr_type::e_fcmp:
            tmp =
                create_expr<FCmpExpr>(as_a<FCmpExpr>(ve)->get_fcmp_op(),
                                      phi_lhs->get_val(i), phi_rhs->get_val(i));
            break;
        default:
            assert(false);
        }
        auto pout_i = _pout[phi_lhs->get_pre_bb(i)];
        auto vn = getVN(pout_i, tmp);
        if (vn == nullptr)
            vn = valuePhiFunc(tmp);
        if (vn == nullptr)
            return nullptr;
        else {
            res_phi->add_val(vn);
        }
    }
    return res_phi;
}

std::shared_ptr<GVN::Expression> GVN::get_ve(Value *val, partitions &pin) {
    for (auto cc : pin) {
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
    for (auto &[bb_r, part] : non_copy_pout) { // if it is a copy statement, it
                                               // shouldn't replace any inst
        auto bb = bb_r;
        for (auto &cc : part) {
            if (cc->index == 0)
                continue;
            for (auto &member : cc->members) {
                if (member != cc->leader and not ::is_a<Constant>(member)) {
                    assert(cc->leader);
                    if (::is_a<PhiInst>(cc->leader) &&
                        ::is_a<PhiInst>(member) &&
                        ::as_a<PhiInst>(member)->get_parent() !=
                            ::as_a<PhiInst>(cc->leader)->get_parent()) {
                        continue;
                    }
                    member->replace_all_use_with_if(
                        cc->leader, [bb](const Use &use) -> bool {
                            if (auto inst =
                                    dynamic_cast<Instruction *>(use.user)) {
                                auto parent = inst->get_parent();
                                if (::is_a<PhiInst>(inst))
                                    return inst->get_operand(use.op_idx + 1) ==
                                           bb; // only replace the
                                               // operand of the
                                               // user from current
                                               // bb for phi
                                else
                                    return parent == bb; // replace the members
                                                         // if users are in the
                                                         // same block as bb
                            }
                            return false;
                        });
                }
            }
        }
    }
    return;
}
