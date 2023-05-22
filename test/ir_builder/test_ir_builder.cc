#include "ast.hh"
#include "ir_builder.hh"
#include "raw_ast.hh"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

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
    auto tmp = [&](const string &suffix) {
        return tmp_path / (test_case + suffix);
    };

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

    ofstream ir{tmp(".ll")};
    ir << module->print();
    ir.close();

    auto clang_cmd = "clang -Wno-override-module " + tmp(".ll").string() + " " +
                     lib_file.string() + " -o " + tmp("").string();
    auto clang_exit_code = std::system(clang_cmd.c_str());

    if (clang_exit_code != 0) {
        throw runtime_error{"clang failed to compile the ll file"};
    }

    auto bin_cmd = tmp("").string() + " 1> " + tmp(".stdout").string() +
                   " 2> " + tmp(".stderr").string();
    if (is_regular_file(test(in_path, ".in"))) {
        bin_cmd += " < " + test(in_path, ".in").string();
    }
    size_t bin_exit_code = WEXITSTATUS(std::system(bin_cmd.c_str()));

    ifstream bin_stdout{tmp(".stdout")};
    ofstream bin_out{tmp(".out")};

    // copy stdout to buf
    stringstream bufs{};
    bufs << bin_stdout.rdbuf();
    bin_stdout.close();

    // output buf exitcode to .out
    auto buf = bufs.str();
    bin_out << buf;
    if (not buf.empty() and buf.back() != '\n') {
        bin_out << '\n';
    }
    bin_out << bin_exit_code << '\n';
    bin_out.close();

    auto diff_cmd =
        "diff " + tmp(".out").string() + " " + test(out_path, ".out").string();
    auto diff_exit_code = std::system(diff_cmd.c_str());

    if (diff_exit_code != 0) {
        throw runtime_error{"the output is incorrect"};
    }
}
