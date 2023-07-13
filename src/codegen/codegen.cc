#include "codegen.hh"
#include "context.hh"

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

    Context context{Stage::stage1, Role::Full, c._allocator};

    c._mir_module->dump(os, context);
    return os;
}
