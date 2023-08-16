#pragma once

#include "err.hh"
#include "module.hh"
#include "utils.hh"

#include <any>
#include <list>
#include <sys/cdefs.h>
#include <typeindex>

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
  public:
    explicit Pass() = default;
    Pass(const Pass &) = delete;
    Pass &operator=(const Pass &) = delete;

    virtual ~Pass() {}
    virtual bool run(PassManager *mgr) = 0;
    virtual void get_analysis_usage(AnalysisUsage &AU) const {}
    virtual bool always_invalid() const { return false; }
};

class AnalysisUsage {
    friend class PassManager;

  public:
    enum KillType { Normal, All, None };

  private:
    // for conservation, all analysis result may be killed
    KillType _kt{All};
    // passes in _relys must be executed in advance
    PassOrder _relys{};
    // passes in _posts may be executed depending on PM
    PassOrder _posts{};
    // after the host pass run, the results of _kills is invalidate
    PassOrder _kills{};
    // may need a preserve list

    void clear() {
        _relys.clear();
        _posts.clear();
        _kills.clear();
    }

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
    explicit AnalysisPass() = default;
    virtual ~AnalysisPass() { clear(); }

    virtual void get_analysis_usage(AnalysisUsage &AU) const override {
        using KillType = AnalysisUsage::KillType;
        AU.set_kill_type(KillType::None);
    }

    // clear resources
    virtual void clear(){};

    virtual std::any get_result() const = 0;
};

class TransformPass : public Pass {
  public:
    explicit TransformPass() = default;

    virtual void get_analysis_usage(AnalysisUsage &AU) const override {
        using KillType = AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
    }
    virtual bool always_invalid() const override { return true; }
};

class IterativePass : public Pass {
  public:
    explicit IterativePass() = default;
};

class PassManager {
    class PassInfo {
        Ptr<Pass> ptr;
        bool valid;
        const bool aws_inv; // invalid is always true for some TransformPass
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
    Ptr<ir::Module> _m; // irbuilder should transfer control to PM
    PassOrder _order;
    std::list<PassIDType> _pass_record;

  public:
    PassManager(Ptr<ir::Module> &&m) : _m(std::move(m)) {}

    // for an instance of PassManager, each pass should be added at most once
    // This function must be defined in header.
    template <typename PassName, typename... Args>
    void add_pass(Args &&...args) {
        auto ID = PassID<PassName>();
        if (not contains(_passes, ID)) {
            _passes.insert(
                {ID, PassInfo(new PassName(std::forward<Args>(args)...))});
        }
        _order.push_back(ID);
    }

    template <typename PassName,
              typename ResultType = typename PassName::ResultType>
    const ResultType &get_result() {
        auto ID = PassID<PassName, AnalysisPass>();
        PassInfo &info = at(ID);
        if (info.need_run()) {
            run({ID}, false);
        }
        auto reuslt_ptr = as_a<const AnalysisPass>(info.get())->get_result();
        return *std::any_cast<const ResultType *>(reuslt_ptr);
    }

    void run(const PassOrder &o, bool post = true);
    void run_iteratively(const PassOrder &order);

    void reset() {
        for (auto &[_, info] : _passes)
            info.mark_killd();
    }

    // FIXME: how to return a const Module* for AnalysisPass?
    ir::Module *get_module() { return _m.get(); }

    Ptr<ir::Module> release_module() { return std::move(_m); }

    std::string print_passes_runned() const;

  private:
    PassInfo &at(PassIDType id) {
        try {
            PassInfo &info = _passes.at(id);
            return info;
        } catch (const std::out_of_range &) {
            std::string pass_name = demangle(id.name());
            throw std::logic_error{"Pass " + pass_name +
                                   " is not added, make sure add_pass<" +
                                   pass_name + ">() has been called"};
        }
    }

    bool run_single_pass(PassIDType passid, bool force, bool post);
};
}; // namespace pass
