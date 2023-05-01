#include "function.hh"
#include "type.hh"
#include "basic_block.hh"
#include <string>
#include "module.hh"
using namespace ir;

Function::~Function(){
    for(auto arg : _args){
        delete arg;
    }
}

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

std::string Function::print() const{
    std::string func_ir;
    func_ir = "define " + this->get_type()->print() + " " + this->get_name();
    func_ir += "(";
    for(auto& arg : this->_args){
        func_ir += arg->get_type()->print() + ":" + arg->print() + ",";
    }
    func_ir[func_ir.size()-1] = ')';
    func_ir += "}";
    for(auto& bb : this->_bbs){
        func_ir += bb.print();
    }
    func_ir += "}";
    return func_ir;
}

const std::vector<Argument*> &Function::get_args() { return _args; }
void Function::add_arg(Argument *arg) { _args.push_back(arg); }
void Function::add_basic_block(BasicBlock *bb) { _bbs.push_back(bb); }
std::string Argument::print() const {
    return this->get_name();
}