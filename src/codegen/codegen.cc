#include "codegen.hh"

using namespace std;
using namespace codegen;
using namespace mir;

std::ostream &codegen::operator<<(std::ostream &os, const CodeGen &c) {
    switch (c._stage) {
    case mir::Stage::stage1:
        os << "# stage1, uncomplete asm\n";
        break;
    case mir::Stage::stage2:
        os << "# stage2, complete asm\n";
        break;
    }
    c.mir_module->dump(os, c._stage);
    return os;
}
