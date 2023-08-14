#include "module.hh"
#include "pass.hh"

#include <any>
#include <iostream>
#include <set>
#include <string>

using namespace pass;
using namespace ir;
using namespace std;

class DeadCodeElim;
class Dominator;
class Pass1;
class Pass2;

class Dominator : public AnalysisPass {
  public:
    struct ResultType {
        // NOTE: Just Simulation
        std::map<BasicBlock *, std::set<BasicBlock *>> doms{}; // dominance set
        std::map<BasicBlock *, BasicBlock *> idom{}; // immediate dominance
        std::map<BasicBlock *, std::set<BasicBlock *>>
            dom_frontier{}; // dominance frontier set
        std::map<BasicBlock *, std::set<BasicBlock *>> dom_tree_succ_blocks{};
        std::string final_result;
    };

    Dominator() = default;

    virtual void get_analysis_usage(AnalysisUsage &AU) const override {
        using KillType = AnalysisUsage::KillType;
        AU.set_kill_type(KillType::None);
        AU.add_require<Pass1>();
        AU.add_require<Pass2>();
    }

    virtual std::any get_result() const override { return &_result; }

    virtual bool run(PassManager *mgr) override final {
        // NOTE: use this to make sure module will not be modified :(
        /* const Module *m = mgr->get_module();
         * m->create_func(); */
        clear();
        cout << "running Dominator" << endl;
        _result.final_result = "Result Of Dominator";
        return false;
    }

    virtual void clear() override final { _result.final_result = ""; }

  private:
    ResultType _result;
};

class Mem2reg : public TransformPass {
  public:
    Mem2reg() = default;

    virtual void get_analysis_usage(AnalysisUsage &AU) const override {
        using KillType = AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
        AU.add_require<Dominator>();
        AU.add_post<DeadCodeElim>();
    }

    virtual bool run(PassManager *mgr) override {
        auto &reuslt = mgr->get_result<Dominator>();
        // reuslt.final_result = "adw";

        cout << "running Mem2reg, get Dominator result: " << reuslt.final_result
             << endl;
        return false;
    }
};

class DeadCodeElim : public TransformPass {
  public:
    DeadCodeElim() = default;

    virtual bool always_invalid() const override { return true; }
    virtual bool run(PassManager *mgr) override {
        cout << "running DeadCodeElim" << endl;
        return false;
    }
};

class Pass1 : public AnalysisPass {
  public:
    struct ResultType {};

    virtual std::any get_result() const override { return &result; }

    virtual bool run(PassManager *mgr) override final {
        cout << "running Pass1" << endl;
        return false;
    }

  private:
    ResultType result;
};

class Pass2 : public AnalysisPass {
  public:
    struct ResultType {};

    virtual std::any get_result() const override { return &result; }

    virtual bool run(PassManager *mgr) override final {
        cout << "running Pass2" << endl;
        return false;
    }

    virtual void get_analysis_usage(AnalysisUsage &AU) const override {
        using KillType = AnalysisUsage::KillType;
        AU.set_kill_type(KillType::None);
        AU.add_require<Pass1>();
    }

  private:
    ResultType result;
};

int main() {
    /* The rely graph of passes:
     *
     * Pass1 <-- Pass2 <-- Dominator <-- Mem2reg ::> DeadCodeElim(suggested)
     *    ^----------------+
     */
    PassManager pm(nullptr);

    pm.add_pass<Pass1>();
    pm.add_pass<Pass2>();
    pm.add_pass<DeadCodeElim>();
    pm.add_pass<Dominator>();
    pm.add_pass<Mem2reg>();

    // we don't want a suggested post pass to run now
    pm.reset();
    cout << "===Test2===" << endl;
    pm.run({PassID<Mem2reg>()}, false);

    // run the core pass mem2reg, should bring life to Dominator and DCE
    pm.reset();
    cout << "===Test3===" << endl;
    pm.run({PassID<Mem2reg>()});
}
