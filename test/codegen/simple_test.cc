#include "ast.hh"
#include "codegen.hh"
#include "dominator.hh"
#include "ir_builder.hh"
#include "mem2reg.hh"
#include "pass.hh"
#include "raw_ast.hh"
#include "remove_unreach_bb.hh"
#include "usedef_chain.hh"
#include <filesystem>
#include <fstream>

using namespace std;
using namespace filesystem;

int main(int argc, char **argv) {
    if (argc != 7) {
        throw std::runtime_error{"wrong arguement"};
    }

    auto test_case = string{argv[1]};

    // tmp folder for binary and output
    auto tmp_path = path{argv[2]};
    create_directories(tmp_path);
    /* auto tmp = [&](const string &suffix) {
     *     return tmp_path / (test_case + suffix);
     * };  */

    // paths that store the testcases
    auto sysy_path = path{argv[3]};
    auto in_path = path{argv[4]};
    auto out_path = path{argv[5]};
    auto test = [&](const path &base, const string &suffix) {
        return base / (test_case + suffix);
    };

    auto lib_file = path{argv[6]};

    if (not is_regular_file(test(sysy_path, ".sy"))) {
        throw runtime_error{"sysy source not exist"};
    }

    ast::AST ast{ast::RawAST{test(sysy_path, ".sy")}};
    IRBuilder builder{ast};

    auto module = builder.release_module();
    pass::PassManager pm(std::move(module));
    pm.add_pass<pass::RmUnreachBB>();
    pm.add_pass<pass::Dominator>();
    pm.add_pass<pass::UseDefChain>();
    pm.add_pass<pass::Mem2reg>();
    pm.run({pass::PassID<pass::Mem2reg>()});

    codegen::CodeGen codegen{pm.release_module()};
    cout << codegen;
}
