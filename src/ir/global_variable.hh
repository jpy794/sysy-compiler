#ifndef IR_GLOBALVARIABLE
#define IR_GLOBALVARIABLE
class GlobalVariable:public Value{
    public:
        ~GlobalVariable()=default;
        static GlobalVariable* get(Type* type, Constant* init, std::string &name, Module* parent);
        Constant* get_init() const {return init_;};
    private:
        GlobalVariable(Type* type, Constant* init, std::string &name, Module* parent);
        Constant* init_;
        Module* parent_;
};
#endif