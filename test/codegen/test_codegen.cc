#include "codegen.hh"
#include "ir_builder.hh"
#include "raw_ast.hh"
#include <filesystem>
#include <ostream>

using namespace std;
using namespace filesystem;

int main(int argc, char **argv) {
    if (argc < 7) {
        throw std::runtime_error{"wrong arguement"};
    }

    auto test_case = string{argv[1]};

    // tmp folder for binary and output
    auto output_path = path{argv[2]};
    create_directories(output_path);

    // paths that store the testcases
    auto sysy_path = path{argv[3]};
    auto test = [&](const path &base, const string &suffix) {
        return base / (test_case + suffix);
    };

    if (not is_regular_file(test(sysy_path, ".sy"))) {
        throw runtime_error{"sysy source not exist"};
    }

    ast::AST ast{ast::RawAST{test(sysy_path, ".sy")}};
    IRBuilder builder{ast};
    codegen::CodeGen codegen{builder.release_module()};

    ofstream asm_output{output_path / (test_case + ".s")};
    asm_output << codegen;
    asm_output.close();

    return 0;
}
