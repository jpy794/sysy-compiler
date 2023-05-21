#include "pass.hh"
#include <set>
#include <stdexcept>

using namespace std;
using namespace pass;

void PassManager::run(bool post, const PassOrder &o) {
    // if the user doesn't define an order, just use the previous order
    const PassOrder &order = (o.size() ? o : _order);

    for (auto passid : order) {
        PassInfo &info = at(passid);
        Pass *ptr = info.get();
        if (not info.need_run())
            continue;
        AnalysisUsage AU;
        ptr->get_analysis_usage(AU);

        // recursively run relied pass
        for (auto relyid : AU._relys) {
            if (at(relyid).need_run())
                run(false, {relyid});
        }

        ptr->run(this);

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
                run(true, {postid});

        info.mark_valid();
    }
}
