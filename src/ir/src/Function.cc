
Function::Function(FuncType* type, std::string& name, Module* parent)
    : Value(type, name), parent_(parent), seq_cnt_(0) {
        parent->add_function(this);
        for(unsigned i=0;i<type->get_num_params();i++){
            add_arg(new Argument(type->get_param_type(i), "", i));
        }
    }
Function::~Function(){
    for (auto *arg : args_)
        delete arg;
    for (auto *bb : bbs_)
        delete bb;
}
static Function* Function::create(FuncType* type, std::string& name, Module* parent){
    return new Function(type, name, parent);
}
Type* Function::get_return_type() const {
    return static_cast<FuncType*>(get_type())->get_result_type();
}
const list<Argument*>& Function::get_args() {
    return args_;
}
void Function::add_arg(Argument* arg) {
    args_.push_back(arg);
}
void Function::add_basic_block(BasicBlock* bb) {
    bbs_.push_back(bb);
}