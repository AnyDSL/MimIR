#include <cstdlib>
#include <cstring>

#include <fstream>
#include <iostream>
#include <lyra/lyra.hpp>

#include "thorin/config.h"

#include "cli/dialects.h"
#include "thorin/be/dot/dot.h"
#include "thorin/be/ll/ll.h"
#include "thorin/fe/parser.h"
#include "thorin/pass/pass.h"
#include "thorin/util/stream.h"

#ifdef _WIN32
#    include <windows.h>
#    define popen  _popen
#    define pclose _pclose
#else
#    include <dlfcn.h>
#endif

using namespace thorin;
using namespace std::literals;

static const auto version = "thorin command-line utility version " THORIN_VER "\n";

/// see https://stackoverflow.com/a/478960
static std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) { throw std::runtime_error("popen() failed!"); }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) { result += buffer.data(); }
    return result;
}

static std::string get_clang_from_path() {
    std::string clang;
#ifndef _WIN32
    clang = exec("which clang");
#else
    clang = exec("where clang");
#endif
    clang.erase(std::remove(clang.begin(), clang.end(), '\n'), clang.end());
    return clang;
}

static std::ostream* open(std::ofstream& stream, const std::string& name) {
    if (name == "-") return &std::cout;
    stream.open(name);
    return &stream;
}
int main(int argc, char** argv) {
    std::string clang = get_clang_from_path();
    std::string file;
    bool emit_llvm   = false;
    bool emit_dot    = false;
    bool emit_thorin = false;
    bool show_help   = false;
    std::vector<std::string> dialects, dialect_paths;
    std::vector<size_t> breakpoints;
    std::string log_level = "error";
    std::string log_name  = "error";

    auto print_version = [](bool) {
        std::cerr << version;
        std::exit(EXIT_SUCCESS);
    };

    constexpr const char* Levels = "error|warn|info|verbose|debug";
    // clang-format off
    auto cli = lyra::cli()
        | lyra::help(show_help)
        | lyra::opt(clang,         "clang"   )["-c"]["--clang"       ]("path to clang executable (default: " + clang + ")")
        | lyra::opt(emit_dot                 )      ["--emit-dot"    ]("emit Graphviz DOT")
        | lyra::opt(emit_llvm                )      ["--emit-llvm"   ]("emit LLVM")
        | lyra::opt(emit_thorin              )      ["--emit-thorin" ]("emit Thorin")
        | lyra::opt(print_version            )["-v"]["--version"     ]("display version info and exit")
        | lyra::opt(dialects,      "dialect" )["-d"]["--dialect"     ]("dynamically load dialect [WIP]")
        | lyra::opt(dialect_paths, "path"    )["-D"]["--dialect-path"]("path to search dialects in")
        | lyra::opt(log_level,     Levels    )      ["--log-level"   ]("set log level").choices("error", "warn", "info", "verbose", "debug")
        | lyra::opt(log_name,      "log name")      ["--log"         ]("file name of the output log; use '-' to output to stdout")
#ifndef NDEBUG
        | lyra::opt(breakpoints,   "gid"     )["-b"]["--break"       ]("trigger break-point upon construction of node with global id <gid>")
#endif
        | lyra::arg(file, "input file")("input file; use '-' to read from stdin");
    // clang-format off

    if (auto result = cli.parse({argc, argv}); !result) {
        std::cerr << "Error in command line: " << result.message() << std::endl;
        return EXIT_FAILURE;
    }

    if (show_help) std::cerr << cli << "\n";

    try {
        if (!dialects.empty()) {
            for (const auto& dialect : dialects) test_plugin(dialect, dialect_paths);
            return EXIT_SUCCESS;
        }

        if (file.empty()) throw std::logic_error("no input file given");

        World world;
#ifndef NDEBUG
        for (auto b : breakpoints) world.breakpoint(b);
#endif

        std::ofstream log_stream;
        if (!log_name.empty()) world.set_log_stream(std::make_shared<thorin::Stream>(*open(log_stream, log_name)));
        world.set_log_level(log_level);

        if (file == "-") {
            Parser parser(world, "<stdin>", std::cin);
            parser.parse_module();
        } else {
            std::ifstream ifs(file);
            if (!ifs) {
                errln("error: cannot read file '{}'", file);
                return EXIT_FAILURE;
            }
            Parser parser(world, file, ifs);
            parser.parse_module();
        }

        if (emit_thorin) world.dump();

        if (emit_dot) {
            std::ofstream of("test.dot");
            Stream s(of);
            dot::emit(world, s);
        }
        if (emit_llvm) {
            std::ofstream of("test.ll");
            Stream s(of);
            ll::emit(world, s);
        }
    } catch (const std::exception& e) {
        errln("{}", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "error: unknown exception" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
