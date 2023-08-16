#include "constant.hh"
#include "instruction.hh"
#include "type.hh"
#include "utils.hh"
#include "value.hh"
#include <array>
#include <memory>

namespace Matcher {

using namespace ir;
template <class T> using Ptr = std::shared_ptr<T>;

class Matcher {
  protected:
  public:
    virtual bool match(Value *) = 0;
    virtual ~Matcher(){};
};

class ConstIntMatcher final : public Matcher {
    int &value;

  public:
    ConstIntMatcher(int &val) : value(val) {}
    bool match(Value *v) override final {
        if (is_a<ConstInt>(v)) {
            value = as_a<ConstInt>(v)->val();
            return true;
        }
        if (is_a<ConstZero>(v) and v->get_type()->is<IntType>()) {
            value = 0;
            return true;
        }
        return false;
    }
};
inline Ptr<ConstIntMatcher> is_cint_like(int &v) {
    return std::make_shared<ConstIntMatcher>(v);
};

class SpecificConstInt final : public Matcher {
    const int value;

  public:
    SpecificConstInt(int v) : value(v) {}
    bool match(Value *v) override final {
        if (is_a<ConstInt>(v) and value == as_a<ConstInt>(v)->val())
            return true;
        if (value == 0 and is_a<ConstZero>(v) and v->get_type()->is<IntType>())
            return true;
        return false;
    }
};
inline Ptr<SpecificConstInt> is_cint(int v) {
    return std::make_shared<SpecificConstInt>(v);
}

class Any final : public Matcher {
    Value *&value;

  public:
    Any(Value *&val) : value(val) {}
    bool match(Value *v) override final {
        value = v;
        return true;
    }
};
inline Ptr<Any> any_val(Value *&val) { return std::make_shared<Any>(val); }

class OneUse final : public Matcher {
    Ptr<Matcher> matcher;

  public:
    OneUse(Ptr<Matcher> matcher) : matcher(matcher) {}
    bool match(Value *v) override final {
        return v->get_use_list().size() == 1 and matcher->match(v);
    }
};
inline Ptr<OneUse> one_use(Ptr<Matcher> m) {
    return std::make_shared<OneUse>(m);
}

class SameVal final : public Matcher {
    Value *value;

  public:
    SameVal(Value *val) : value(val) {}
    bool match(Value *other) override final { return value == other; }
};
inline Ptr<SameVal> same(Value *val) { return std::make_shared<SameVal>(val); }

class IBinaryMatcher : public Matcher {
    const bool commutative;
    IBinaryInst::IBinOp op;
    Ptr<Matcher> lhs_matcher;
    Ptr<Matcher> rhs_matcher;

  public:
    IBinaryMatcher(IBinaryInst::IBinOp op, Ptr<Matcher> lm, Ptr<Matcher> rm,
                   bool commutative = false)
        : commutative(commutative), op(op), lhs_matcher(lm), rhs_matcher(rm) {}

    bool match(Value *v) override final {
        if (not is_a<IBinaryInst>(v))
            return false;
        auto bin_inst = as_a<IBinaryInst>(v);
        if (bin_inst->get_ibin_op() == op) {
            if (lhs_matcher->match(bin_inst->lhs()) and
                rhs_matcher->match(bin_inst->rhs()))
                return true;
            if (commutative and rhs_matcher->match(bin_inst->lhs()) and
                lhs_matcher->match(bin_inst->rhs()))
                return true;
        }
        return false;
    }
};
inline Ptr<IBinaryMatcher> iadd(Ptr<Matcher> lm, Ptr<Matcher> rm) {
    return std::make_shared<IBinaryMatcher>(IBinaryInst::ADD, lm, rm, true);
}
inline Ptr<IBinaryMatcher> isub(Ptr<Matcher> lm, Ptr<Matcher> rm) {
    return std::make_shared<IBinaryMatcher>(IBinaryInst::SUB, lm, rm, false);
}
inline Ptr<IBinaryMatcher> imul(Ptr<Matcher> lm, Ptr<Matcher> rm) {
    return std::make_shared<IBinaryMatcher>(IBinaryInst::MUL, lm, rm, true);
}
inline Ptr<IBinaryMatcher> idiv(Ptr<Matcher> lm, Ptr<Matcher> rm) {
    return std::make_shared<IBinaryMatcher>(IBinaryInst::SDIV, lm, rm, false);
}

}; // namespace Matcher
