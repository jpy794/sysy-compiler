#include "function.hh"
#include "basic_block.hh"
#include "module.hh"
#include "type.hh"

using namespace ir;
using namespace std;

Function::Function(FuncType *type, std::string &&name, bool external)
    : Value(type, "@" + name), is_external(external), _inst_seq(0) {
    for (size_t i = 0; i < type->get_param_types().size(); ++i) {
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
    if (this->is_external) {
        func_ir = "declare";
    } else {
        func_ir = "define";
    }
    func_ir +=
        " " +
        dynamic_cast<FuncType *>(this->get_type())->get_result_type()->print() +
        " " + this->get_name();
    func_ir += "(";
    for (auto &arg : this->_args) {
        func_ir += arg->get_type()->print() + " " + arg->get_name() + ", ";
    }
    if (this->_args.size() > 0)
        func_ir.erase(func_ir.length() - 2, 2);
    func_ir += ")";
    if (!this->is_external) {
        func_ir += "{\n";
        for (auto &bb : this->_bbs) {
            func_ir += bb.print();
        }
        func_ir += "}";
    }
    return func_ir;
}

std::string Argument::print() const { return this->get_name(); }
