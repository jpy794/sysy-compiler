#include "pass.hh"
#include <set>
#include <stdexcept>

using namespace std;
using namespace pass;

void PassManager::run(const PassOrder &o, bool post) {
    // ignore _order and always use order provided by user
    // or it could be confusing when user tries to provide an empty order list
    const PassOrder &order = o;

    for (auto passid : order) {
        PassInfo &info = at(passid);
        Pass *ptr = info.get();
        if (not info.need_run())
            continue;
        AnalysisUsage AU;
        ptr->get_analysis_usage(AU); // get relyed pass

        // recursively run relied pass
        for (auto relyid : AU._relys) {
            if (at(relyid).need_run())
                run({relyid}, false);
        }

        ptr->run(this);
        _pass_record.push_back(passid);

        AU.clear();

        ptr->get_analysis_usage(AU); // get passes killed and suggest post

        // invalidation of affected passes
        switch (AU._kt) {
        case AnalysisUsage::Normal:
            for (auto killid : AU._kills) {
                if (contains(_passes, killid))
                    at(killid).mark_killd();
            }
            break;
        case AnalysisUsage::All:
            for (auto &[_, passinfo] : _passes)
                if (is_a<AnalysisPass>(passinfo.get()))
                    passinfo.mark_killd();
            break;
        case AnalysisUsage::None:
            break;
        }

        if (post)
            for (auto postid : AU._posts)
                run({postid}, true);

        info.mark_valid();
    }
}

string PassManager::print_passes_runned() const {
    string ret{"passes runned: "};
    for (auto passid : _pass_record) {
        string pass_name = demangle(passid.name());
        ret += pass_name + " ";
    }
    return ret;
}
