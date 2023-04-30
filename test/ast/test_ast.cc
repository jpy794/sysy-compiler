#include "ast.hh"
#include "raw_ast.hh"
#include <fstream>
#include <stdexcept>

using namespace ast;
using namespace std;

int main(int argc, char **argv) {
    if (argc != 3) {
        throw std::runtime_error{"wrong arguement"};
    }
    auto input = argv[1];
    auto output = argv[2];

    AST ast{RawAST{input}};
    ofstream out{output};
    out << ast;
}