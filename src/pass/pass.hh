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

class Pass;
class AnalysisUsage;
class AnalysisPass;
class TransformPass;
class PassManager;

template <typename T> using Ptr = std::unique_ptr<T>;
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
};

class TransformPass : public Pass {
  public:
    explicit TransformPass(ir::Module *m, PassManager *mgr) : Pass(m, mgr) {}

    virtual void get_analysis_usage(AnalysisUsage &AU) const override {
        using KillType = AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
    }
};

class PassManager {
    class PassInfo {
        Pass *ptr;
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
        Pass *get() { return ptr; }
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
        // initialize for Singleton
        if (PassName::get(false) == nullptr)
            PassName::initialize(_m, this, args...);
        // add to PM's pass-info-map
        auto ID = PassID<PassName>();
        if (not contains(_passes, ID)) {
            _passes.insert({ID, PassInfo(PassName::get())});
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
        return PassName::get()->_result;
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

// Note: use the macro below to get focused on the implemention
// - friend class for Singleton
// - BaseType initialization
#define __Default_Head(PassName, BaseType)                                     \
  private:                                                                     \
    friend class Singleton<PassName>;                                          \
    PassName(ir::Module *m, PassManager *mgr) : BaseType(m, mgr) {}

// should used after the defination of ResultType
// - let PM's get_result can access the _result
// - declare the analysis result `_result`
// - make the public domain constant(define of ResultType must be in public)
#define Analysis_Default_HEAD(PassName)                                        \
    __Default_Head(PassName, AnalysisPass);                                    \
    friend const ResultType &PassManager::get_result<PassName>();              \
    ResultType _result;                                                        \
                                                                               \
  public:

//
#define Transform_Default_HEAD(PassName) __Default_Head(PassName, TransformPass)

}; // namespace pass
