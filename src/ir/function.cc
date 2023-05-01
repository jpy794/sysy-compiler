#include "function.hh"

using namespace ir;

Function::Function(FuncType *type, std::string &name, Module *parent)
    : Value(type, name), _args(), _bbs(), _parent(parent), _seq_cnt(0) {
    parent->add_function(this);
    for (unsigned i = 0; i < type->get_num_params();) {
        add_arg(new Argument(type->get_param_type(i), this));
    }
}

Function *Function::create(FuncType *type, std::string &name, Module *parent) {
    return new Function(type, name, parent);
}

Type *Function::get_return_type() const {
    return static_cast<const FuncType *>(this->get_type())->get_result_type();
}

const ilist<Argument> &Function::get_args() { return _args; }
void Function::add_arg(Argument *arg) { _args.push_back(arg); }
void Function::add_basic_block(BasicBlock *bb) { _bbs.push_back(bb); }