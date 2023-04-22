#ifndef IR_CONSTANT
#define IR_CONSTANT
class Constant: public Value{
    public:
        Constant(Type* type, const std::string &name="", unsigned num_ops = 0);
        ~Constant=defalut;
};
#endif