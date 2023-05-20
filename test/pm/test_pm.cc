#include "basic_block.hh"
#include "module.hh"
#include "pass.hh"
#include "utils.hh"

#include <any>
#include <iostream>
#include <set>
#include <string>

using namespace pass;
using namespace ir;
using namespace std;

class DeadCodeElim;
class Dominator;

class Dominator : public AnalysisPass, public Singleton<Dominator> {

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

    Analysis_Default_HEAD(Dominator);

    virtual void run() override final {
        clear();
        cout << "running Dominator" << endl;
        _result.final_result = "Result Of Dominator";
    }

    virtual void clear() override final { _result.final_result = ""; }
};

class Mem2reg : public TransformPass, public Singleton<Mem2reg> {
    Transform_Default_HEAD(Mem2reg);

  public:
    virtual void get_analysis_usage(AnalysisUsage &AU) const override {
        using KillType = AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
        AU.add_require<Dominator>();
        AU.add_post<DeadCodeElim>();
    }

    virtual void run() override {
        auto &reuslt = get_result<Dominator>();
        // reuslt.final_result = "cannot modify the data";

        cout << "running Mem2reg, get Dominator result: " << reuslt.final_result
             << endl;
    }
};

class DeadCodeElim : public TransformPass, public Singleton<DeadCodeElim> {
    Transform_Default_HEAD(DeadCodeElim);

  public:
    virtual bool always_invalid() const override { return true; }
    virtual void run() override { cout << "running DeadCodeElim" << endl; }
};

int main() {
    PassManager pm(nullptr);

    pm.add_pass<DeadCodeElim>();
    pm.add_pass<Dominator>();
    pm.add_pass<Mem2reg>();

    // default case, use add order
    cout << "===Test1===" << endl;
    pm.run();

    // we don't want a suggested post pass to run now
    pm.reset();
    cout << "===Test2===" << endl;
    pm.run(false);

    pm.reset();
    // run the core pass mem2reg, should bring life to Dominator and DCE
    cout << "===Test3===" << endl;
    pm.run(true, {PassID<Mem2reg>()});
}
