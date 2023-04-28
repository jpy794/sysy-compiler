#include <cstdlib>
#include <exception>
#include <iostream>

#include "antlr4-runtime.h"
#include "sysyLexer.h"
#include "sysyParser.h"

#include "ast.hh"
#include "raw_ast.hh"
#include "astprinter.hh"

using namespace std;
using namespace antlr4;


int main(int argc, const char *argv[]) {
    auto filename = argv[1];
    std::ifstream stream;
    stream.open(argv[1]);

    ANTLRInputStream input(stream);
    sysyLexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    sysyParser parser(&tokens);

    tree::ParseTree *tree = parser.compUnit();
    cout << tree->toStringTree() << endl;

    ASTPrinter printer;

    try{
        ast::AST myast(filename);
    } catch (exception &e)
    {
        cout << e.what() << endl;
        exit(-1);
    }
    myast.visit(&printer);

    try {
        print
    } catch (declaration) {
    
    }  

    return 0;
}
