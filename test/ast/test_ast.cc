#include "ast.hh"
#include "raw_ast.hh"
#include <filesystem>
#include <fstream>
#include <stdexcept>

using namespace ast;
using namespace std;
using namespace filesystem;

int main(int argc, char **argv) {
    if (argc != 3) {
        throw std::runtime_error{"wrong arguement"};
    }
    auto input = argv[1];
    auto output = argv[2];

    auto out_path = path(output).remove_filename();
    create_directories(out_path);

    AST ast{RawAST{input}};
    ofstream out{output};
    out << ast;
}