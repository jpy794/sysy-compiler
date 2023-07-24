#include "DeadCode.hh"
#include "ast.hh"
#include "codegen.hh"
#include "dominator.hh"
#include "func_info.hh"
#include "ir_builder.hh"
#include "loop_find.hh"
#include "loop_invariant.hh"
#include "mem2reg.hh"
#include "pass.hh"
#include "raw_ast.hh"
#include "remove_unreach_bb.hh"
#include "usedef_chain.hh"
#include <filesystem>
#include <fstream>

using namespace std;
using namespace filesystem;
using namespace pass;

int main(int argc, char **argv) {
    if (argc != 2) {
        throw std::runtime_error{"wrong arguement"};
    }

    auto case_full_path = path{argv[1]};

    auto name_without_extension = string(case_full_path.stem());
    auto category = case_full_path.parent_path().stem();

    auto output_path = path{path("output") / category};
    create_directories(output_path);

    if (not exists(case_full_path)) {
        throw runtime_error{"sysy source not exist"};
    }

    ast::AST ast{ast::RawAST{case_full_path}};
    IRBuilder builder{ast};

    // optimize
    pass::PassManager pm(builder.release_module());
    // analysis
    pm.add_pass<Dominator>();
    pm.add_pass<UseDefChain>();
    pm.add_pass<LoopFind>();
    pm.add_pass<FuncInfo>();
    pm.add_pass<DepthOrder>();

    // transform
    pm.add_pass<RmUnreachBB>();
    pm.add_pass<Mem2reg>();
    pm.add_pass<LoopInvariant>();
    pm.add_pass<DeadCode>();

    pm.run({PassID<Mem2reg>(), PassID<LoopInvariant>(), PassID<DeadCode>()},
           false);

    // codegen, output stage1 asm only
    codegen::CodeGen codegen{pm.release_module(), true};
    ofstream asm_output{output_path / (name_without_extension + ".s")};
    asm_output << codegen;
    asm_output.close();

    cout << codegen;

    return 0;
}
