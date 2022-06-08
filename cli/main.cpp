#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <lyra/lyra.hpp>

#include "thorin/config.h"
#include "thorin/dialects.h"

#include "thorin/be/dot/dot.h"
#include "thorin/fe/parser.h"
#include "thorin/pass/optimize.h"
#include "thorin/pass/pass.h"
#include "thorin/pass/pipelinebuilder.h"
#include "thorin/util/sys.h"

using namespace thorin;
using namespace std::literals;

static const auto version = "thorin command-line utility version " THORIN_VER "\n";

int main(int argc, char** argv) {
    try {
        static constexpr const char* Backends = "thorin|h|md|ll|dot";

        std::string input, prefix;
        std::string clang = sys::find_cmd("clang");
        std::vector<std::string> dialect_names, dialect_paths, emitters;
        std::vector<size_t> breakpoints;

        bool emit_thorin  = false;
        bool emit_h       = false;
        bool emit_md      = false;
        bool emit_ll      = false;
        bool emit_dot     = false;
        bool show_help    = false;
        bool show_version = false;

        int verbose      = 0;
        auto inc_verbose = [&](bool) { ++verbose; };

        // clang-format off
        auto cli = lyra::cli()
            | lyra::help(show_help)
            | lyra::opt(show_version             )["-v"]["--version"     ]("Display version info and exit.")
            | lyra::opt(clang,         "clang"   )["-c"]["--clang"       ]("Path to clang executable (default: '" THORIN_WHICH " clang').")
            | lyra::opt(dialect_names, "dialect" )["-d"]["--dialect"     ]("Dynamically load dialect [WIP].")
            | lyra::opt(dialect_paths, "path"    )["-D"]["--dialect-path"]("Path to search dialects in.")
            | lyra::opt(emitters,      Backends  )["-e"]["--emit"        ]("Select emitter. Multiple emitters can be specified simultaneously.").choices("thorin", "h", "md", "ll", "dot")
            | lyra::opt(inc_verbose              )["-V"]["--verbose"     ]("Verbose mode. Multiple -V options increase the verbosity. The maximum is 4.").cardinality(0, 4)
#if THORIN_ENABLE_CHECKS
            | lyra::opt(breakpoints,   "gid"     )["-b"]["--break"       ]("Trigger breakpoint upon construction of node with global id <gid>. Useful when running in a debugger.")
#endif
            | lyra::opt(prefix,        "prefix"  )["-o"]["--output"      ]("Prefix used for various output files.")
            | lyra::arg(input,         "file"    )                        ("Input file.");

        if (auto result = cli.parse({argc, argv}); !result) throw std::invalid_argument(result.message());

        if (show_help) {
            std::cout << cli;
            return EXIT_SUCCESS;
        }

        if (show_version) {
            std::cerr << version;
            std::exit(EXIT_SUCCESS);
        }

        for (const auto& e : emitters) {
            if (false) {}
            else if (e == "thorin") emit_thorin = true;
            else if (e == "h" )     emit_h      = true;
            else if (e == "md")     emit_md     = true;
            else if (e == "ll")     emit_ll     = true;
            else if (e == "dot")    emit_dot    = true;
            else unreachable();
        }
        // clang-format on

        // we always need core and mem, as long as we are not in bootstrap mode..
        if (!emit_h) dialect_names.insert(dialect_names.end(), {"core", "mem"});

        std::vector<Dialect> dialects;
        thorin::Backends backends;
        thorin::Normalizers normalizers;
        if (!dialect_names.empty()) {
            for (const auto& dialect : dialect_names) {
                dialects.push_back(Dialect::load(dialect, dialect_paths));
                dialects.back().register_backends(backends);
                dialects.back().register_normalizers(normalizers);
            }
        }

        if (input.empty()) throw std::invalid_argument("error: no input given");
        if (input[0] == '-' || input.substr(0, 2) == "--")
            throw std::invalid_argument("error: unknown option " + input);

        if (prefix.empty()) {
            auto filename = std::filesystem::path(input).filename();
            if (filename.extension() != ".thorin")
                throw std::invalid_argument("error: invalid file name '" + input + "'");
            prefix = filename.stem().string();
        }

        World world;
        world.set_log_ostream(&std::cerr);
        world.set_log_level((LogLevel)verbose);
#if THORIN_ENABLE_CHECKS
        for (auto b : breakpoints) world.breakpoint(b);
#endif

        std::ifstream ifs(input);
        if (!ifs) {
            errln("error: cannot read file '{}'", input);
            return EXIT_FAILURE;
        }

        std::ofstream md;
        if (emit_md) md.open(prefix + ".md");

        Parser parser(world, input, ifs, dialect_paths, &normalizers, emit_md ? &md : nullptr);
        parser.parse_module();

        if (emit_h) {
            std::ofstream h(prefix + ".h");
            parser.bootstrap(h);
        }

        PipelineBuilder builder;
        for (const auto& dialect : dialects) { dialect.register_passes(builder); }

        optimize(world, builder);

        if (emit_thorin) world.dump();

        if (emit_dot) {
            std::ofstream ofs(prefix + ".dot");
            dot::emit(world, ofs);
        }

        if (emit_ll) {
            if (auto it = backends.find("ll"); it != backends.end()) {
                std::ofstream ofs(prefix + ".ll");
                it->second(world, ofs);
            } else
                errln("error: 'll' emitter not loaded. Try loading 'mem' dialect.");
        }
    } catch (const std::exception& e) {
        errln("{}", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        errln("error: unknown exception");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
