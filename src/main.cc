#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <istream>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

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
#include "usedef_chain.hh"

using namespace std;
using namespace filesystem;
using namespace pass;

class Config {
  public:
    bool emit_llvm{false}; // emit llvm or asm
    bool optimize{false};
    string in;
    optional<string> out;

    Config(int argc, char **argv) : args(argv + 1, argv + argc) {
        auto emit_asm = is_cmd_option_exist("-S");
        if (not emit_asm) {
            throw runtime_error{"only support emit asm / llvm"};
        }
        emit_llvm = is_cmd_option_exist("-emit-llvm");
        optimize = is_cmd_option_exist("-O1");
        out = get_cmd_option("-o");
        // expect one and only one source file
        if (args.size() != 1) {
            throw runtime_error{"expect one source file"};
        }
        in = args[0];
    }

  private:
    vector<char *> args;

    optional<string> get_cmd_option(const string &option) {
        auto it = find(args.begin(), args.end(), option);
        if (it == args.end() || it + 1 == args.end()) {
            return nullopt;
        }
        auto ret = *(it + 1);
        args.erase(it, it + 2);
        return ret;
    }

    bool is_cmd_option_exist(const string &option) {
        auto it = find(args.begin(), args.end(), option);
        if (it == args.end()) {
            return false;
        }
        args.erase(it);
        return true;
    }
};

int main(int argc, char **argv) {
    Config cfg{argc, argv};

    if (not is_regular_file(cfg.in)) {
        throw runtime_error{"source file does not exist"};
    }

    { // [DEBUG] input file
        auto filename = filesystem::path{cfg.in}.filename();
        debugs << "=========Debug Info For " << filename << "=========\n";
    }

    ast::AST ast{ast::RawAST{cfg.in}};

    IRBuilder builder{ast};
    auto module = builder.release_module();

    PassManager pm{std::move(module)};

    // analysis
    pm.add_pass<Dominator>();
    pm.add_pass<UseDefChain>();
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

    if (cfg.optimize) {
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
                PassID<AlgebraicSimplify>(),
                PassID<LoopInvariant>(),
                PassID<LoopUnroll>(),
                PassID<AlgebraicSimplify>(),
                // PassID<GVN>(), // can u be run please?
                PassID<ControlFlow>(), 
                PassID<DeadCode>(),
            },
            true);
    } else
        pm.run({PassID<Mem2reg>(), PassID<DeadCode>()});

    { // [DEBUG] runned passes
        debugs << pm.print_passes_runned() << "\n";
    }
    module = pm.release_module();

    // output
    ostream *os{nullptr};
    if (cfg.out.has_value()) {
        os = new ofstream{cfg.out.value()};
    } else {
        os = &cout;
    }

    if (cfg.emit_llvm) {
        // emit llvm
        auto ll = module->print();
        *os << ll;
    } else {
        // emit asm
        codegen::CodeGen codegen{std::move(module), cfg.optimize};
        *os << codegen;
    }

    if (cfg.out.has_value()) {
        delete os;
    }

    return 0;
}
