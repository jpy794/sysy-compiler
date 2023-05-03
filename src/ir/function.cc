#include "function.hh"
#include "basic_block.hh"
#include "module.hh"
#include "type.hh"

using namespace ir;
using namespace std;

Function::Function(Module *module, FuncType *type, std::string &&name)
    : Value(module, type, std::move(name)), _inst_seq(0) {
    for (size_t i = 0; i < type->get_num_params(); ++i) {
        _args.push_back(new Argument(this, type->get_param_type(i)));
    }
}

Function::~Function() {
    for (auto arg : _args) {
        delete arg;
    }
}

std::string Function::print() const {
    std::string func_ir;
    func_ir =
        "define " +
        dynamic_cast<FuncType *>(this->get_type())->get_result_type()->print() +
        " " + print_op(this);
    func_ir += "(";
    for (auto &arg : this->_args) {
        func_ir += arg->get_type()->print() + " " + print_op(arg) + ", ";
    }
    if (this->_args.size() > 0)
        func_ir.erase(func_ir.length() - 2, 2);
    func_ir += ")";
    func_ir += "{\n";
    for (auto &bb : this->_bbs) {
        func_ir += bb.print();
    }
    func_ir += "}";
    return func_ir;
}

std::string Argument::print() const { return this->get_name(); }
