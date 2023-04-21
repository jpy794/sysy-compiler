#ifndef IR_TYPE
#define IR_TYPE
class Type{
    public:
        enum TypeID{
            Int,
            Float,
            Void,
            Label,
            Function,
            Pointer,
            Array
        };
        Type(TypeID tid, Module* m);
        ~Type = defalut;
        bool is_int_type() const { return tid_==Int; }
        bool is_float_type() const { return tid_==Float; }
        bool is_void_type() const { return tid_==Void; }
        bool is_label_type() const { return tid_==Label; }
        bool is_function_type() const { return tid_==Function; }
        bool is_pointer_type() const { return tid_==Pointer; }
        bool is_array_type() const { return tid_==Array; }
    private:
        TypeID tid_;
        Module *m_;
}
#endif