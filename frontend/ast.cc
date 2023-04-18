#include "ast.hh"
#include "sysyVisitor.h"

#include <any>
#include <iostream>

using namespace std;
using namespace ast;

class ASTBuilder : public sysyVisitor {
    any visitExp(sysyParser::ExpContext *ctx) override { return {}; }
};

AST::AST(const string &src_file) { std::cout << "hello AST\n"; }
