#ifndef IR_FUNCTION
#define IR_FUNCTION
class Function:public Value{
    public:
        Function(Type* ret_type, std::vector<Type*> param_types, std::string& name, Module* parent);
        ~Function=defalut;
    private:
        std::vector<Value*> args_;
        std::list<BasicBlock*> bbs_;
        Module* parent_;
};
#endif