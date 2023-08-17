#include "function.hh"
#include "basic_block.hh"
#include "instruction.hh"
#include "module.hh"
#include "type.hh"
#include <cassert>

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

void Function::decay_to_void_ret() {
    auto func_type = as_a<FuncType>(get_type());
    assert(not func_type->get_result_type()->is<VoidType>());
    auto void_type = Types::get().void_type();
    auto param_type = func_type->get_param_types();
    auto void_func_type =
        Types::get().func_type(void_type, std::move(param_type));
    change_type(void_func_type);
    // rm ret value
    for (auto &bb : _bbs) {
        auto ret = &*bb.insts().rbegin();
        if (not is_a<RetInst>(ret))
            continue;
        assert(ret->operands().size() == 1);
        ret->remove_operand(0);
    }
}
