#ifndef IR_MODULE
#define IR_MODULE
class Module{
    public:
        Module(string &name);
        ~Module=defalut;
    private:
        std::string module_name_;
};
#endif