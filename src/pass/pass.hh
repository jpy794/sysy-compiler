#pragma once

#include "err.hh"
#include "module.hh"
#include "utils.hh"

#include <any>
#include <cassert>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

#include <iostream>

namespace pass {

// NOTE: The explanation below may have expired, I will rewrite/delete it then.
/* @ Pass Design
 * 1. A Pass may rely on other Passes' result, so `add_require<>()` is provided
 * 2. A Pass may have a will for other Passes to be executed after its own run,
 *    so a `add_post<>()` is provided, it will be treated as a suggestion.
 */
class Pass;

/* @ AnalysisUsage Design
 */
class AnalysisUsage;

/* @ AnalysisPass Design
 * 1. Subclasses of AnalysisPass should define its return type by:
 *      `using ResultType = ...`
 *    and set the return value after run() is called
 *    This is a convention, so the PassManager can use
 *    AnalysisPassSubClass::ResultType for any_cast
 */
class AnalysisPass;

/* @ TransformPass Design
 * 1. Cause TransformPass may change IR, so TransformPass shuold have a `cancel`
 *    function to denote a AnalysisPass set for cancelization
 */
class TransformPass;

/* @ PassManager Design
 * 1. Auto satisfy the data rely during run() is called
 * 2. Register mechanism, this
 * 1. For one instance of PassManager, each pass should be added at most once
 */
class PassManager;

template <typename T> using Ptr = std::unique_ptr<T>;
using AnalysisResultType = std::any;
using PassIDType = std::type_index;
using PassOrder = std::list<PassIDType>;

template <typename PassName, typename Base = Pass> PassIDType PassID() {
    static_assert(std::is_base_of<Base, PassName>::value);
    return PassIDType(typeid(PassName));
}

class Pass {
  protected:
    ir::Module *_m;
    PassManager *_mgr;

  public:
    explicit Pass(ir::Module *m, PassManager *mgr) : _m(m), _mgr(mgr) {}
    Pass(const Pass &) = delete;
    Pass &operator=(const Pass &) = delete;

    // FIXME: Shall we set the return type to bool to indicate if a pass
    // modifies the IR?
    virtual void run() = 0;
    virtual void get_analysis_usage(AnalysisUsage &AU) const {}
    virtual bool always_invalid() const { return false; }

  protected:
    // shouldn't change the result
    template <typename RequireType>
    const typename RequireType::ResultType &get_result();
};

class AnalysisUsage {
    friend class PassManager;

  public:
    enum KillType { Normal, All, None };

  private:
    // for conservation, all analysis result may be killed
    KillType _kt{All};
    // passes in _relys must be executed in advance
    PassOrder _relys;
    // passes in _posts may be executed depending on PM
    PassOrder _posts;
    // after the host pass run, the results of _kills is invalidate
    PassOrder _kills;
    // may need a preserve list

  public:
    AnalysisUsage() = default;

    template <typename RequireType> void add_require() {
        _relys.push_back(PassID<RequireType>());
    }

    template <typename RequireType> void add_post() {
        _posts.push_back(PassID<RequireType>());
    }

    template <typename RequireType> void add_kill() {
        _kills.push_back(PassID<RequireType>());
    }

    void set_kill_type(KillType kt) { _kt = kt; }
};

class AnalysisPass : public Pass {
  public:
    explicit AnalysisPass(ir::Module *m, PassManager *mgr) : Pass(m, mgr) {}
    virtual ~AnalysisPass() { clear(); }

    virtual void get_analysis_usage(AnalysisUsage &AU) const override {
        using KillType = AnalysisUsage::KillType;
        AU.set_kill_type(KillType::None);
    }

    // clear resources
    virtual void clear(){};

    virtual const void *get_result() const = 0;
};

class TransformPass : public Pass {
  public:
    explicit TransformPass(ir::Module *m, PassManager *mgr) : Pass(m, mgr) {}

    /* void run() override { std::cout << "in TransformPass" << std::endl; } */
    virtual void get_analysis_usage(AnalysisUsage &AU) const override {
        using KillType = AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
    }
};

class PassManager {
    /* public:
     * private: */
    class PassInfo {
        Ptr<Pass> ptr;
        bool valid;
        const bool aws_inv; // invalid is always false for some TransformPass
      public:
        PassInfo(Pass *p)
            : ptr(p), valid(false), aws_inv(p->always_invalid()) {}
        PassInfo(PassInfo &&pi) = default;
        // PassInfo &operator=(PassInfo &&pi) = default;

        void mark_killd() { valid = false; }
        void mark_valid() { valid = (not aws_inv) and true; }
        bool need_run() const { return not valid; }
        Pass *get() { return ptr.get(); }
    };

    std::map<PassIDType, PassInfo> _passes;
    ir::Module *_m;
    PassOrder _order;

  public:
    PassManager(ir::Module *m) : _m(m) {}

    // for an instance of PassManager, each pass should be added at most once
    // This function must be defined in header.
    template <typename PassName, typename... Args>
    void add_pass(Args &&...args) {
        auto ID = PassID<PassName>();
        if (not contains(_passes, ID)) {
            _passes.insert({ID, PassInfo(new PassName(
                                    _m, this, std::forward<Args>(args)...))});
        }
        _order.push_back(ID);
    }

    template <typename PassName,
              typename ResultType = typename PassName::ResultType>
    const ResultType &get_result() {
        auto ID = PassID<PassName, AnalysisPass>();
        PassInfo &info = _passes.at(ID);
        if (info.need_run()) {
            run(false, {ID});
        }
        auto reuslt_ptr = as_a<const AnalysisPass>(info.get())->get_result();
        return *(static_cast<const ResultType *>(reuslt_ptr));
    }

    void run(bool post = true, const PassOrder &o = {});

    void reset() {
        for (auto &[_, info] : _passes)
            info.mark_killd();
    }
};

template <typename RequireType>
inline const typename RequireType::ResultType &Pass::get_result() {
    return _mgr->get_result<RequireType>();
}

}; // namespace pass
