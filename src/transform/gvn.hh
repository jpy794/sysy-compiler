#pragma once
#include "basic_block.hh"
#include "constant.hh"
#include "dead_code.hh"
#include "depth_order.hh"
#include "func_info.hh"
#include "instruction.hh"
#include "mem2reg.hh"
#include "pass.hh"
#include "utils.hh"
#include "value.hh"
#include <cassert>
#include <cstddef>
#include <list>
#include <memory>
#include <set>
#include <utility>
#include <vector>

// TODO:analysis of MemAddress use chain
// improve efficiency of GVN

namespace pass {

class GVN final : public pass::TransformPass {
  public:
    GVN() = default;
    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::Normal);
        AU.add_require<FuncInfo>();
        AU.add_require<DepthOrder>();
        AU.add_post<DeadCode>();
    }
    virtual bool run(pass::PassManager *mgr) override;

    template <class T, typename... Args>
    std::shared_ptr<T> create_expr(Args... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    bool always_invalid() const override { return false; }

    // Expression
    class Expression {
      public:
        enum class expr_type {
            e_const,
            e_unique,
            e_call,
            e_unit,
            e_ibin,
            e_fbin,
            e_icmp,
            e_fcmp,
            e_gep,
            e_phi,
            e_load,
            e_store
        };
        Expression(expr_type op) : _op(op) {}
        expr_type get_op() const { return _op; }
        virtual std::string print() = 0;
        template <typename T>
        static bool equal_as(const Expression *lhs, const Expression *rhs) {
            return *::as_a<const T>(lhs) == *::as_a<const T>(rhs);
        }

        bool operator==(const Expression &other) const {
            if (this->get_op() != other.get_op())
                return false;
            if (this == &other)
                return true;
            bool cmp_resu = true;
            switch (_op) {
            case expr_type::e_const:
                cmp_resu = equal_as<ConstExpr>(this, &other);
                break;
            case expr_type::e_unique:
                cmp_resu = equal_as<UniqueExpr>(this, &other);
                break;
            case expr_type::e_call:
                cmp_resu = equal_as<CallExpr>(this, &other);
                break;
            case expr_type::e_unit:
                cmp_resu = equal_as<UnitExpr>(this, &other);
                break;
            case expr_type::e_ibin:
                cmp_resu = equal_as<IBinExpr>(this, &other);
                break;
            case expr_type::e_fbin:
                cmp_resu = equal_as<FBinExpr>(this, &other);
                break;
            case expr_type::e_icmp:
                cmp_resu = equal_as<ICmpExpr>(this, &other);
                break;
            case expr_type::e_fcmp:
                cmp_resu = equal_as<FCmpExpr>(this, &other);
                break;
            case expr_type::e_gep:
                cmp_resu = equal_as<GepExpr>(this, &other);
                break;
            case expr_type::e_phi:
                cmp_resu = equal_as<PhiExpr>(this, &other);
                break;
            case expr_type::e_load:
                cmp_resu = equal_as<LoadExpr>(this, &other);
                break;
            case expr_type::e_store:
                cmp_resu = equal_as<StoreExpr>(this, &other);
                break;
            }
            return cmp_resu;
        }

      private:
        expr_type _op;
    };
    friend Expression;
    // ConstExpr and UniqueExpr will be as the basic ValueExpression for other
    // Expr that takes Expr as args
    class ConstExpr final : public Expression {
      public:
        ConstExpr(ir::Value *con)
            : Expression(expr_type::e_const),
              _const(::as_a<ir::Constant>(con)) {}

        bool operator==(const ConstExpr &other) const {
            return _const == other._const;
        }

        virtual std::string print() {
            return "ConstExpr:" + _const->get_name() + "";
        }

      private:
        ir::Constant *_const;
    };
    // the case, that operands of the current inst doesn't have ValExpr, won't
    // happen in depth_priority_oder
    class UniqueExpr final : public Expression { // load/store/alloca/ptr2int/int2ptr
      public:
        UniqueExpr(ir::Value *val)
            : Expression(expr_type::e_unique), _val(val) {}

        bool operator==(const UniqueExpr &other) const {
            return _val == other._val;
        }

        virtual std::string print() {
            return "UniqueExpr:" + _val->get_name() + "\n";
        }

      private:
        ir::Value *_val;
    };
    // For non-pure functions, it will be as the basic ValueExpression
    class CallExpr final : public Expression {
      public:
        CallExpr(ir::Instruction *inst)
            : Expression(expr_type::e_call), _inst(inst) {} // non-pure function
        CallExpr(ir::Function *func,
                 std::vector<std::shared_ptr<Expression>> &&params)
            : Expression(expr_type::e_call), _func(func), _params(params) {
        } // pure function

        bool operator==(const CallExpr &other) const {
            if (_func == nullptr)
                return _inst == other._inst;
            else if (_func != other._func ||
                     _params.size() != other._params.size())
                return false;
            else
                for (unsigned i = 0; i < _params.size(); i++) {
                    if (not(*_params[i] == *other._params[i]))
                        return false;
                }
            return true;
        }

        virtual std::string print() {
            if (_func != nullptr) {
                return "CallExpr:{\n" + _func->get_name() + "}";
            }
            return "CallExpr:{\n" + _inst->get_name() + "}";
        }

      private:
        ir::Function *_func{};
        ir::Instruction *_inst{};
        std::vector<std::shared_ptr<Expression>> _params{};
    };
    class LoadExpr final : public Expression {
      public:
        LoadExpr(std::shared_ptr<Expression> addr)
            : Expression(expr_type::e_load), _addr(addr) {}

        bool operator==(const LoadExpr &other) const {
            return false && *_addr == *other._addr;
        }

        virtual std::string print() {
            return "LoadExpr:{\n" + _addr->print() + "}";
        }

      private:
        std::shared_ptr<Expression> _addr;
    };

    class StoreExpr final : public Expression {
      public:
        StoreExpr(std::shared_ptr<Expression> val,
                  std::shared_ptr<Expression> addr)
            : Expression(expr_type::e_store), _val(val), _addr(addr) {}

        bool operator==(const StoreExpr &other) const {
            return false && *_addr == *other._addr &&
                   *_val == *other._val; // TODO: how to tell two StoreExpr is
                                         // the same
        }

        virtual std::string print() {
            return "StoreExpr:{\n" + _addr->print() + "," + _val->print() + "}";
        }

      private:
        std::shared_ptr<Expression> _val;
        std::shared_ptr<Expression> _addr;
    };

    // If two UnitExprs' _unit is different, the exprs are
    // absolutely different
    class UnitExpr final : public Expression { // zext/fp2si/si2fp/sext/trunc/
      public:
        UnitExpr(std::shared_ptr<Expression> oper)
            : Expression(expr_type::e_unit), _unit(oper) {}

        bool operator==(const UnitExpr &other) const {
            return *_unit == *other._unit;
        }

        virtual std::string print() {
            return "UnitExpr:{\n" + _unit->print() + "}";
        }

      private:
        std::shared_ptr<Expression> _unit;
    };

    class BinExpr : public Expression {
      public:
        BinExpr(expr_type ty, std::shared_ptr<Expression> lhs,
                std::shared_ptr<Expression> rhs)
            : Expression(ty), _lhs(lhs), _rhs(rhs) {}
        std::shared_ptr<Expression> get_lhs() const { return _lhs; }
        std::shared_ptr<Expression> get_rhs() const { return _rhs; }

      private:
        std::shared_ptr<Expression> _lhs, _rhs;
    };

    class IBinExpr final : public BinExpr {
      public:
        IBinExpr(ir::IBinaryInst::IBinOp op, std::shared_ptr<Expression> lhs,
                 std::shared_ptr<Expression> rhs)
            : BinExpr(expr_type::e_ibin, lhs, rhs), _op(op) {}

        bool operator==(const IBinExpr &other) const {
            if (_op == ir::IBinaryInst::ADD or _op == ir::IBinaryInst::MUL)
                return _op == other._op && ((*get_lhs() == *other.get_lhs() &&
                                             *get_rhs() == *other.get_rhs()) ||
                                            (*get_lhs() == *other.get_rhs() &&
                                             *get_rhs() == *other.get_lhs()));
            return _op == other._op && *get_lhs() == *other.get_lhs() &&
                   *get_rhs() == *other.get_rhs();
        }

        ir::IBinaryInst::IBinOp get_ibin_op() { return _op; }

        virtual std::string print() {
            std::string lhs_s = "[" + get_lhs()->print() + "]\n";
            std::string rhs_s = "[" + get_rhs()->print() + "]\n";
            return "IBin:{\n" + lhs_s + rhs_s + "}\n";
        }

      private:
        ir::IBinaryInst::IBinOp _op;
    };

    class FBinExpr final : public BinExpr {
      public:
        FBinExpr(ir::FBinaryInst::FBinOp op, std::shared_ptr<Expression> lhs,
                 std::shared_ptr<Expression> rhs)
            : BinExpr(expr_type::e_fbin, lhs, rhs), _op(op) {}

        bool operator==(const FBinExpr &other) const {
            if (_op == ir::FBinaryInst::FADD or _op == ir::FBinaryInst::FMUL)
                return _op == other._op && ((*get_lhs() == *other.get_lhs() &&
                                             *get_rhs() == *other.get_rhs()) ||
                                            (*get_lhs() == *other.get_rhs() &&
                                             *get_rhs() == *other.get_lhs()));
            return _op == other._op && *get_lhs() == *other.get_lhs() &&
                   *get_rhs() == *other.get_rhs();
        }

        ir::FBinaryInst::FBinOp get_fbin_op() { return _op; }

        virtual std::string print() {
            std::string lhs_s = "[" + get_lhs()->print() + "]\n";
            std::string rhs_s = "[" + get_rhs()->print() + "]\n";
            return "FBin:{\n" + lhs_s + rhs_s + "}\n";
        }

      private:
        ir::FBinaryInst::FBinOp _op;
    };

    class ICmpExpr final : public BinExpr {
      public:
        ICmpExpr(ir::ICmpInst::ICmpOp op, std::shared_ptr<Expression> lhs,
                 std::shared_ptr<Expression> rhs)
            : BinExpr(expr_type::e_icmp, lhs, rhs), _op(op) {}

        bool operator==(const ICmpExpr &other) const {
            return _op == other._op && *get_lhs() == *other.get_lhs() &&
                   *get_rhs() == *other.get_rhs();
        }

        ir::ICmpInst::ICmpOp get_icmp_op() { return _op; }

        virtual std::string print() {
            std::string lhs_s = "[" + get_lhs()->print() + "]\n";
            std::string rhs_s = "[" + get_rhs()->print() + "]\n";
            return "ICmp:{\n" + lhs_s + rhs_s + "}\n";
        }

      private:
        ir::ICmpInst::ICmpOp _op;
    };

    class FCmpExpr final : public BinExpr {
      public:
        FCmpExpr(ir::FCmpInst::FCmpOp op, std::shared_ptr<Expression> lhs,
                 std::shared_ptr<Expression> rhs)
            : BinExpr(expr_type::e_fcmp, lhs, rhs), _op(op) {}

        bool operator==(const FCmpExpr &other) const {
            return _op == other._op && *get_lhs() == *other.get_lhs() &&
                   *get_rhs() == *other.get_rhs();
        }

        ir::FCmpInst::FCmpOp get_fcmp_op() { return _op; }

        virtual std::string print() {
            std::string lhs_s = "[" + get_lhs()->print() + "]\n";
            std::string rhs_s = "[" + get_rhs()->print() + "]\n";
            return "FCmp:{\n" + lhs_s + rhs_s + "}\n";
        }

      private:
        ir::FCmpInst::FCmpOp _op;
    };

    class GepExpr final : public Expression {
      public:
        GepExpr(std::vector<std::shared_ptr<Expression>> &&idxs)
            : Expression(expr_type::e_gep), _idxs(idxs) {}

        bool operator==(const GepExpr &other) const {
            if (this->_idxs.size() != other._idxs.size())
                return false;
            for (unsigned i = 0; i < _idxs.size(); i++) {
                if (not(*_idxs[i] == *other._idxs[i]))
                    return false;
            }
            return true;
        }

        virtual std::string print() { return "Gep"; }

      private:
        std::vector<std::shared_ptr<Expression>> _idxs;
    };

    class PhiExpr final : public Expression {
      public:
        PhiExpr(ir::BasicBlock *bb)
            : Expression(expr_type::e_phi), _ori_bb(bb),
              _pre_bbs({bb->pre_bbs().begin(), bb->pre_bbs().end()}), _vals{} {}
        PhiExpr(ir::BasicBlock *bb,
                std::vector<std::shared_ptr<Expression>> &&vals)
            : Expression(expr_type::e_phi), _ori_bb(bb),
              _pre_bbs({bb->pre_bbs().begin(), bb->pre_bbs().end()}),
              _vals(vals) {}

        size_t size() const { return _vals.size(); }

        ir::BasicBlock *get_ori_bb() { return _ori_bb; }

        std::shared_ptr<Expression> get_val(size_t i) { return _vals[i]; }

        ir::BasicBlock *get_pre_bb(size_t i) { return _pre_bbs[i]; }

        void add_val(std::shared_ptr<Expression> ve) { _vals.push_back(ve); }

        bool operator==(const PhiExpr &other) const {
            if (_ori_bb != other._ori_bb)
                return false;
            for (unsigned i = 0; i < _vals.size(); i++) {
                assert(not(_vals[i] == nullptr || other._vals[i] == nullptr));
                if (not(*_vals[i] == *other._vals[i]))
                    return false;
            }
            return true;
        }

        virtual std::string print() {
            std::string val_s{};
            for (auto val : _vals) {
                val_s += "[" + val->print() + "]";
            }
            return "Phi:{\n" + val_s + "}\n";
        }

      private:
        ir::BasicBlock *_ori_bb;
        std::vector<ir::BasicBlock *> _pre_bbs;
        std::vector<std::shared_ptr<Expression>> _vals;
    };

    // CongruemceClass
    size_t next_value_number;
    struct CongruenceClass {
        unsigned index;
        ir::Value *leader;
        std::shared_ptr<Expression> val_expr;
        std::shared_ptr<PhiExpr> phi_expr;
        std::set<ir::Value *> members;
        bool operator<(const CongruenceClass &other) const {
            return this->index < other.index;
        }
        bool operator==(const CongruenceClass &other) const;

        CongruenceClass(size_t index)
            : index(index), leader{}, val_expr{}, phi_expr{}, members{} {}
        CongruenceClass(size_t index, ir::Value *leader,
                        std::shared_ptr<Expression> val_expr,
                        std::shared_ptr<PhiExpr> phi_expr)
            : index(index), leader(leader), val_expr(val_expr),
              phi_expr(phi_expr), members{} {}
        CongruenceClass(size_t index, ir::Value *leader,
                        std::shared_ptr<Expression> val_expr,
                        std::shared_ptr<PhiExpr> phi_expr, ir::Value *member)
            : index(index), leader(leader), val_expr(val_expr),
              phi_expr(phi_expr), members{member} {}
    };
    template <typename... Args>
    static std::shared_ptr<CongruenceClass>
    create_cc(Args... args) { // only the number of TOP cc can be 0
        return std::make_shared<CongruenceClass>(std::forward<Args>(args)...);
    }
    // partitions
    struct less_part {
        bool
        operator()(const std::shared_ptr<pass::GVN::CongruenceClass> &a,
                   const std::shared_ptr<pass::GVN::CongruenceClass> &b) const {
            return *a < *b;
        }
    };
    using partitions = std::set<std::shared_ptr<CongruenceClass>, less_part>;
    // when transfering partitions between bbs, it must use "clone" in order
    // to prevent from changing other bbs' pout when changing cur_bb's pin
    partitions clone(const partitions &p);
    partitions clone(partitions &&p);

    // GVN functions
    void detect_equivalences(ir::Function *func);
    partitions join(const partitions &p1, const partitions &p2);
    std::shared_ptr<CongruenceClass>
        intersect(std::shared_ptr<CongruenceClass>,
                  std::shared_ptr<CongruenceClass>);
    partitions transfer_function(ir::Instruction *, partitions &);
    std::shared_ptr<Expression> valueExpr(ir::Value *, partitions &);
    std::shared_ptr<PhiExpr> valuePhiFunc(std::shared_ptr<Expression>);
    std::shared_ptr<Expression> getVN(partitions &,
                                      std::shared_ptr<Expression>);

    // replace the members of the same CongruemceClass with the first value
    void replace_cc_members();

    // utils function
    std::shared_ptr<Expression> get_ve(ir::Value *, partitions &);
    template <typename Derived, typename Base>
    static bool is_a(std::shared_ptr<Base> base) {
        static_assert(std::is_base_of<Base, Derived>::value);
        return std::dynamic_pointer_cast<Derived>(base) != nullptr;
    }
    template <typename Derived, typename Base>
    static std::shared_ptr<Derived> as_a(std::shared_ptr<Base> base) {
        static_assert(std::is_base_of<Base, Derived>::value);
        std::shared_ptr<Derived> derived =
            std::dynamic_pointer_cast<Derived>(base);
        if (not derived) {
            throw std::logic_error{"bad asa"};
        }
        return derived;
    }
    template <typename ExprT, typename InstT,
              typename OP> // only for valueExpr function
    std::shared_ptr<ExprT> create_BinOperExpr(OP op, ir::Value *val,
                                              partitions &pin) {
        std::shared_ptr<Expression> lhs, rhs;
        lhs = valueExpr(::as_a<InstT>(val)->get_operand(0), pin);
        rhs = valueExpr(::as_a<InstT>(val)->get_operand(1), pin);
        return create_expr<ExprT>(op, lhs, rhs);
    }

    void clear() {
        _val2expr.clear();
        non_copy_pout.clear();
    }

  private:
    partitions TOP{create_cc(0)};
    ir::Function *_func;
    ir::BasicBlock *_bb;
    const pass::FuncInfo::ResultType *_func_info;
    const pass::DepthOrder::ResultType *_depth_order;
    std::map<ir::BasicBlock *, partitions> _pin, _pout;

    // helper members which can improve analysis efficiency
    std::unordered_map<ir::Value *, std::shared_ptr<Expression>>
        _val2expr{}; // just record GlobalVal and Constant
    unsigned phi_construct_point;
    std::map<ir::BasicBlock *, partitions> non_copy_pout;
};
}; // namespace pass
