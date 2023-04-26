BasicBlock::BasicBlock(Type* type, const std::string &name, Function* parent)
        : Value(type, name), parent_(parent) {parent->add_bb(this);}
static BasicBlock* BasicBlock::create(const std::string &name, Function* parent, Module* m){
    return new BasicBlock(m->get_label_type(), parent, name);
}
void BasicBlock::add_instruction(Instruction* instr){
    instr_list_.push_back(instr);
}