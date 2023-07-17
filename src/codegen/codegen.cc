#include "codegen.hh"
#include "context.hh"
#include "err.hh"
#include "mir_memory.hh"
#include "mir_register.hh"
#include <cassert>

using namespace std;
using namespace codegen;
using namespace mir;
using namespace context;

std::ostream &codegen::operator<<(std::ostream &os, const CodeGen &c) {
    switch (c._stage) {
    case Stage::stage1: {
        os << "# stage1, uncomplete asm\n";
    } break;
    case Stage::stage2:
        os << "# stage2, complete asm\n";
        break;
    }

    Context context{c._stage, Role::Full, c._allocator};

    c._mir_module->dump(os, context);
    return os;
}

void CodeGen::upgrade() {
    _stage = Stage::stage2;
    auto &reg_mgr = PhysicalRegisterManager::get();

    // TODO float
    for (auto func : _mir_module->get_functions()) {
        if (not func->is_definition())
            continue;
        auto &int_reg_map = _allocator.get_reg_map(func, false);
        // auto &int_spilled = _allocator.get_spilled(func, false);

        // TODO resolve frame

        for (auto label : func->get_labels())
            for (auto &inst : label->get_insts()) {
                for (unsigned i = 0; i < inst.get_operand_num(); ++i) {
                    auto op = inst.get_operand(i);
                    if (is_a<VirtualRegister>(op)) {
                        auto vid = as_a<VirtualRegister>(op)->get_id();
                        if (is_a<IVReg>(op)) {
                            auto regid = int_reg_map.at(vid).reg->get_id();
                            inst.set_operand(i, reg_mgr.get_int_reg(regid));
                        } else if (is_a<FVReg>(op)) {
                            throw not_implemented_error{};
                        } else
                            throw unreachable_error{
                                "Register should be virtual here"};
                        // does not consider float now
                        assert(is_a<IVReg>(op));
                    } else if (is_a<StatckObject>(op)) {
                        // TODO 
                    }
                }
            }
    }
}
