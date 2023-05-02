#include "value.hh"
#include "constant.hh"
#include "function.hh"
#include "global_variable.hh"
using std::string;
using namespace ir;
string Value::print_op(const ir::Value *op) const {
    if (dynamic_cast<const ir::Constant *>(op))
        return op->print();
    else if (dynamic_cast<const ir::GlobalVariable *>(op))
        return "@" + op->get_name();
    else if (dynamic_cast<const ir::Function *>(op))
        return "@" + op->get_name();
    else
        return "%" + op->get_name();
}