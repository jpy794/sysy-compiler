#include "algebraic_simplify.hh"
#include "ast.hh"
#include "codegen.hh"
#include "const_propagate.hh"
#include "continuous_addition.hh"
#include "control_flow.hh"
#include "dead_code.hh"
#include "depth_order.hh"
#include "dominator.hh"
#include "err.hh"
#include "func_info.hh"
#include "global_localize.hh"
#include "gvn.hh"
#include "inline.hh"
#include "ir_builder.hh"
#include "log.hh"
#include "loop_find.hh"
#include "loop_invariant.hh"
#include "loop_simplify.hh"
#include "loop_unroll.hh"
#include "mem2reg.hh"
#include "pass.hh"
#include "raw_ast.hh"
#include "strength_reduce.hh"

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

    {
        auto filename = case_full_path.filename();
        debugs << "=========Debug Info For " << filename << "=========\n";
    }

    ast::AST ast{ast::RawAST{case_full_path}};
    IRBuilder builder{ast};

    // optimize
    pass::PassManager pm(builder.release_module());

    // analysis
    pm.add_pass<Dominator>();
    pm.add_pass<LoopFind>();
    pm.add_pass<FuncInfo>();
    pm.add_pass<DepthOrder>();

    // transform
    pm.add_pass<RmUnreachBB>();
    pm.add_pass<Mem2reg>();
    pm.add_pass<LoopSimplify>();
    pm.add_pass<LoopInvariant>();
    pm.add_pass<LoopUnroll>();
    pm.add_pass<ConstPro>();
    pm.add_pass<DeadCode>();
    pm.add_pass<ControlFlow>();
    pm.add_pass<StrengthReduce>();
    pm.add_pass<Inline>();
    pm.add_pass<GVN>();
    pm.add_pass<GlobalVarLocalize>();
    pm.add_pass<ContinuousAdd>();
    pm.add_pass<AlgebraicSimplify>();

    pm.run(
        {
            PassID<GlobalVarLocalize>(),
            PassID<Mem2reg>(),
            PassID<StrengthReduce>(),
            PassID<GVN>(),
            PassID<Inline>(),
            PassID<AlgebraicSimplify>(),
            // PassID<ContinuousAdd>(),
            PassID<LoopInvariant>(),
            PassID<LoopUnroll>(),
            PassID<ControlFlow>(),
        },
        true);
    pm.reset();
    pm.run(
        {
            PassID<ConstPro>(),
            PassID<LoopInvariant>(),
            PassID<AlgebraicSimplify>(),
            PassID<ControlFlow>(),
            PassID<DeadCode>(),
        },
        true);

    // codegen, output stage1 asm only
    codegen::CodeGen codegen{pm.release_module(), false, true};
    ofstream asm_output{output_path / (name_without_extension + ".s")};
    asm_output << codegen;
    asm_output.close();

    cout << codegen;

    return 0;
}
