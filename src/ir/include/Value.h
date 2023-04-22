#ifndef IR_VALUE
#define IR_VALUE
class Value{
    public:
        Value(Type* type, const std::string &name="");
        ~Value=defalut;
    private:
        std::string name_;
        Type* type_;
}
#endif