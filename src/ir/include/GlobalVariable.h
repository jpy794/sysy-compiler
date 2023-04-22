#ifndef IR_GLOBALVARIABLE
#define IR_GLOBALVARIABLE
class GlobalVariable:public Value{
    public:
        GlobalVariable(Type* type, Constant* init, std::string &name, Module* parent);
        ~GlobalVariable=defalut;
    private:
        Constant* init_;
        Module* parent_;
};
#endif