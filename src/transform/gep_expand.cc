#include "gep_expand.hh"
#include "constant.hh"
#include "instruction.hh"

#include <iostream>

using namespace ir;
using namespace std;
using namespace pass;

static auto zero = Constants::get().int_const(0);

bool GEP_Expand::run(PassManager *mgr) {
    bool changed = false;
    for (auto &func : mgr->get_module()->functions()) {
        if (func.is_external)
            continue;
        for (auto &bb : func.bbs()) {
            for (auto &gep : bb.insts()) {
                if (not is_a<GetElementPtrInst>(&gep))
                    continue;
                changed |= expand(as_a<GetElementPtrInst>(&gep));
            }
        }
    }

    return changed;
}

bool GEP_Expand::expand(GetElementPtrInst *gep) {
    if (gep->operands().size() <= 2)
        return false;
    // cout << "expanding gep: " << gep->print() << endl;

    auto bb = gep->get_parent();

    auto baseptr = gep->get_operand(0);
    baseptr =
        bb->insert_inst<GetElementPtrInst>(gep, baseptr, gep->get_operand(1));

    for (unsigned i = 2; i < gep->operands().size(); ++i) {
        auto off = gep->get_operand(i);
        baseptr = bb->insert_inst<GetElementPtrInst>(gep, baseptr, zero, off);
        // cout << "expand result " << i << ": " << baseptr->print() << endl;
    }

    gep->replace_all_use_with(baseptr);
    return true;
}
