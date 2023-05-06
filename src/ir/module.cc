#include "module.hh"

using namespace ir;
using namespace std;

std::string Module::print() const {
    std::string m_ir;
    for (const auto &gv : _global_vars)
        m_ir += gv.print() + "\n";
    for (const auto &func : _funcs)
        m_ir += func.print() + "\n";
    return m_ir;
}
