#pragma once
#include "DeadCode.hh"
#include "basic_block.hh"
#include "constant.hh"
#include "depth_order.hh"
#include "dominator.hh"
#include "func_info.hh"
#include "instruction.hh"
#include "mem2reg.hh"
#include "pass.hh"
#include "usedef_chain.hh"
#include "utils.hh"
#include "value.hh"
#include <cassert>
#include <cstddef>
#include <list>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace pass {

class GVN final : public pass::TransformPass {
  public:
    GVN() = default;
    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::Normal);
        AU.add_require<FuncInfo>();
        AU.add_require<UseDefChain>();
        AU.add_require<DepthOrder>();
        AU.add_require<Mem2reg>();
        AU.add_kill<UseDefChain>();
        AU.add_kill<DeadCode>();
        AU.add_post<DeadCode>();
    }
    virtual void run(pass::PassManager *mgr) override;

    template <class T, typename... Args>
    static std::shared_ptr<T> create_expr(Args... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

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
            e_phi
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
            switch (_op) {
            case expr_type::e_const:
                return equal_as<ConstExpr>(this, &other);
            case expr_type::e_unique:
                return equal_as<UniqueExpr>(this, &other);
            case expr_type::e_call:
                return equal_as<CallExpr>(this, &other);
            case expr_type::e_unit:
                return equal_as<UnitExpr>(this, &other);
            case expr_type::e_ibin:
                return equal_as<IBinExpr>(this, &other);
            case expr_type::e_fbin:
                return equal_as<FBinExpr>(this, &other);
            case expr_type::e_icmp:
                return equal_as<ICmpExpr>(this, &other);
            case expr_type::e_fcmp:
                return equal_as<FCmpExpr>(this, &other);
            case expr_type::e_gep:
                return equal_as<GepExpr>(this, &other);
            case expr_type::e_phi:
                return equal_as<PhiExpr>(this, &other);
            }
        }

      private:
        expr_type _op;
    };
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
            return "ConstExpr(" + _const->get_name() + ")";
        }

      private:
        ir::Constant *_const;
    };
    // when operands of the current inst doesn't have ValExpr, it will be as a
    // temporary Expression which be replaced in later iterations
    class UniqueExpr final
        : public Expression { // load/store/alloca or temporary Expression
      public:
        UniqueExpr(ir::Value *val)
            : Expression(expr_type::e_unique), _val(val) {}

        bool operator==(const UniqueExpr &other) const {
            return _val == other._val;
        }

        virtual std::string print() {
            return "UniqueExpr(" + _val->get_name() + ")";
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
            if (_func != other._func || _params.size() != other._params.size())
                return false;
            for (unsigned i = 0; i < _params.size(); i++) {
                if (not(*_params[i] == *other._params[i]))
                    return false;
            }
            return true;
        }

        virtual std::string print() {
            if (_func != nullptr) {
                return "CallExpr(" + _func->get_name() + ")";
            }
            return "CallExpr(" + _inst->get_name() + ")";
        }

      private:
        ir::Function *_func{};
        ir::Instruction *_inst{};
        std::vector<std::shared_ptr<Expression>> _params{};
    };
    // If two UnitExprs' _unit is different, the exprs are
    // absolutely different
    class UnitExpr final : public Expression { // zext/fp2si/si2fp
      public:
        UnitExpr(std::shared_ptr<Expression> oper)
            : Expression(expr_type::e_unit), _unit(oper) {}

        bool operator==(const UnitExpr &other) const {
            return *_unit == *other._unit;
        }

        virtual std::string print() {
            return "UnitExpr(" + _unit->print() + ")";
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
            return _op == other._op && *get_lhs() == *other.get_lhs() &&
                   *get_rhs() == *other.get_rhs();
        }

        ir::IBinaryInst::IBinOp get_ibin_op() { return _op; }

        virtual std::string print() { return "Bin"; }

      private:
        ir::IBinaryInst::IBinOp _op;
    };

    class FBinExpr final : public BinExpr {
      public:
        FBinExpr(ir::FBinaryInst::FBinOp op, std::shared_ptr<Expression> lhs,
                 std::shared_ptr<Expression> rhs)
            : BinExpr(expr_type::e_fbin, lhs, rhs), _op(op) {}

        bool operator==(const FBinExpr &other) const {
            return _op == other._op && *get_lhs() == *other.get_lhs() &&
                   *get_rhs() == *other.get_rhs();
        }

        ir::FBinaryInst::FBinOp get_fbin_op() { return _op; }

        virtual std::string print() { return "Bin"; }

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

        virtual std::string print() { return "Bin"; }

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

        virtual std::string print() { return "Bin"; }

      private:
        ir::FCmpInst::FCmpOp _op;
    };

    class GepExpr final : public Expression {
      public:
        GepExpr(std::vector<std::shared_ptr<Expression>> &&idxs)
            : Expression(expr_type::e_gep), _idxs(idxs) {}

        bool operator==(const GepExpr &other) const {
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
        PhiExpr() : Expression(expr_type::e_phi), _vals{}, _suc_bbs{} {}
        PhiExpr(std::vector<std::shared_ptr<Expression>> &&vals,
                std::vector<ir::BasicBlock *> &&suc_bbs)
            : Expression(expr_type::e_phi), _vals(vals), _suc_bbs(suc_bbs) {
            assert(vals.size() == suc_bbs.size());
        }
        size_t size() const { return _vals.size(); }
        std::shared_ptr<Expression> get_val(size_t i) { return _vals[i]; }
        ir::BasicBlock *get_suc_bb(size_t i) { return _suc_bbs[i]; }

        void add_val_bb(std::shared_ptr<Expression> ve, ir::BasicBlock *bb) {
            _vals.push_back(ve);
            _suc_bbs.push_back(bb);
        }

        bool operator==(const PhiExpr &other) const {
            for (unsigned i = 0; i < _vals.size(); i++) {
                if (not(*_vals[i] == *other._vals[i]))
                    return false;
            }
            return true;
        }

        virtual std::string print() { return "Phi"; }

      private:
        std::vector<std::shared_ptr<Expression>> _vals;
        std::vector<ir::BasicBlock *> _suc_bbs;
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
    std::shared_ptr<PhiExpr> valuePhiFunc(std::shared_ptr<Expression>,
                                          partitions &);
    std::shared_ptr<Expression> get_ve(ir::Value *, partitions &);
    std::shared_ptr<Expression> getVN(partitions &,
                                      std::shared_ptr<Expression>);

    // replace the members of the same CongruemceClass with the first value
    void replace_cc_members();

    // utils function
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
        auto lhs = valueExpr(::as_a<InstT>(val)->get_operand(0), pin);
        auto rhs = valueExpr(::as_a<InstT>(val)->get_operand(1), pin);
        return create_expr<ExprT>(op, lhs, rhs);
    }

  private:
    partitions TOP{create_cc(0)};
    ir::Function *_func;
    const pass::FuncInfo::ResultType *_func_info;
    const pass::UseDefChain::ResultType *_usedef_chain;
    const pass::DepthOrder::ResultType *_depth_order;
    std::map<ir::BasicBlock *, partitions> _pin, _pout;
};
}; // namespace pass
