#include "algebraic_simplify.hh"
#include "array_visit.hh"
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
#include "func_trim.hh"
#include "gep_expand.hh"
#include "global_localize.hh"
#include "gvn.hh"
#include "inline.hh"
#include "ir_builder.hh"
#include "local_cmnexpr.hh"
#include "log.hh"
#include "loop_find.hh"
#include "loop_invariant.hh"
#include "loop_simplify.hh"
#include "loop_unroll.hh"
#include "mem2reg.hh"
#include "pass.hh"
#include "phi_combine.hh"
#include "raw_ast.hh"
#include "remove_unreach_bb.hh"
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
    pm.add_pass<LoopSimplify>();
    pm.add_pass<LoopInvariant>(); // TODO set changed
    pm.add_pass<ConstPro>();
    pm.add_pass<DeadCode>();
    pm.add_pass<ControlFlow>();
    pm.add_pass<GlobalVarLocalize>();
    pm.add_pass<ContinuousAdd>();
    pm.add_pass<AlgebraicSimplify>();
    pm.add_pass<ArrayVisit>();
    pm.add_pass<LocalCmnExpr>();
    // passes unfit for running iteratively
    pm.add_pass<LoopUnroll>();
    pm.add_pass<Inline>();
    pm.add_pass<Mem2reg>();
    pm.add_pass<GVN>();
    pm.add_pass<FuncTrim>();
    pm.add_pass<PhiCombine>();
    pm.add_pass<GEP_Expand>();

    // the functions from ContinuousAdd and strength_reduce are implemented
    // in algebraic simplify
    PassOrder iterative_passes = {
        PassID<RmUnreachBB>(),   PassID<GlobalVarLocalize>(),
        PassID<ConstPro>(),      PassID<AlgebraicSimplify>(),
        PassID<LoopInvariant>(), PassID<LocalCmnExpr>(),
        PassID<ControlFlow>(),   PassID<ArrayVisit>(),
        PassID<DeadCode>(),      PassID<PhiCombine>(),
    };
    pm.run({PassID<FuncTrim>(), PassID<Mem2reg>()}, true);
    pm.run_iteratively(iterative_passes);
    pm.run({PassID<GVN>()}, true);
    pm.run_iteratively(iterative_passes);
    pm.run({PassID<Inline>()}, true);
    pm.run_iteratively(iterative_passes);
    pm.run({PassID<LoopUnroll>()}, true);
    pm.run_iteratively(iterative_passes);
    pm.run({PassID<GEP_Expand>()}, true);
    pm.run_iteratively(iterative_passes);

    // codegen, output stage1 asm only
    codegen::CodeGen codegen{pm.release_module(), false, true};
    ofstream asm_output{output_path / (name_without_extension + ".s")};
    asm_output << codegen;
    asm_output.close();

    cout << codegen;

    return 0;
}
