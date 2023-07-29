#include "usedef_chain.hh"

using namespace pass;
void UseDefChain::run(pass::PassManager *mgr) {
    clear();
    ir::Module *m = mgr->get_module();
    for (auto &func : m->functions()) {
        for (auto &bb : func.bbs()) {
            for (auto &inst : bb.insts()) {
                auto user = dynamic_cast<ir::User *>(&inst);
                if (_result.users.find(&inst) == _result.users.end()) // init
                    _result.users[&inst] = {};
                for (unsigned idx = 0; idx < inst.operands().size(); idx++) {
                    _result.users[inst.operands()[idx]].push_back({user, idx});
                }
            }
        }
    }
}