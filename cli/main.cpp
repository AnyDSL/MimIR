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

enum Backends { Dot, H, LL, Md, Thorin, Num_Backends };

int main(int argc, char** argv) {
    try {
        static const auto version = "thorin command-line utility version " THORIN_VER "\n";

        bool show_help    = false;
        bool show_version = false;
        std::string input, prefix;
        std::string clang = sys::find_cmd("clang");
        std::vector<std::string> dialect_plugins, dialect_paths;
        std::vector<size_t> breakpoints;
        std::array<std::string, Num_Backends> output;
        int verbose      = 0;
        auto inc_verbose = [&](bool) { ++verbose; };

        // clang-format off
        auto cli = lyra::cli()
            | lyra::help(show_help)
            | lyra::opt(show_version              )["-v"]["--version"      ]("Display version info and exit.")
            | lyra::opt(clang,           "clang"  )["-c"]["--clang"        ]("Path to clang executable (default: '" THORIN_WHICH " clang').")
            | lyra::opt(dialect_plugins, "dialect")["-d"]["--dialect"      ]("Dynamically load dialect [WIP].")
            | lyra::opt(dialect_paths,   "path"   )["-D"]["--dialect-path" ]("Path to search dialects in.")
            | lyra::opt(inc_verbose               )["-V"]["--verbose"      ]("Verbose mode. Multiple -V options increase the verbosity. The maximum is 4.").cardinality(0, 4)
#if THORIN_ENABLE_CHECKS
            | lyra::opt(breakpoints,     "gid"    )["-b"]["--break"        ]("Trigger breakpoint upon construction of node with global id <gid>. Useful when running in a debugger.")
#endif
            | lyra::opt(output[Dot   ],  "file"   )      ["--output-dot"   ]("Emits the Thorin program as a graph using Graphviz' DOT language.")
            | lyra::opt(output[H     ],  "file"   )      ["--output-h"     ]("Emits a header file to be used to interface with a dialect in C++.")
            | lyra::opt(output[LL    ],  "file"   )      ["--output-ll"    ]("Compiles the Thorin program to LLVM.")
            | lyra::opt(output[Md    ],  "file"   )      ["--output-md"    ]("Emits the input formatted as Markdown.")
            | lyra::opt(output[Thorin],  "file"   )      ["--output-thorin"]("Emits the Thorin program again.")
            | lyra::arg(input,           "file"   )                         ("Input file.");
        // clang-format on

        if (auto result = cli.parse({argc, argv}); !result) throw std::invalid_argument(result.message());

        if (show_help) {
            std::cout << cli << std::endl;
            std::cout << "Use \"-\" as <file> to output to stdout." << std::endl;
            return EXIT_SUCCESS;
        }

        if (show_version) {
            std::cerr << version;
            std::exit(EXIT_SUCCESS);
        }

        // prepare output files and streams
        std::array<std::ofstream, Num_Backends> ofs;
        std::array<std::ostream*, Num_Backends> os;
        os.fill(nullptr);
        for (size_t be = 0; be != Num_Backends; ++be) {
            if (output[be].empty()) continue;
            if (output[be] == "-") {
                os[be] = &std::cout;
            } else {
                ofs[be].open(output[be]);
                os[be] = &ofs[be];
            }
        }

        // we always need core and mem, as long as we are not in bootstrap mode..
        if (!os[H]) dialect_plugins.insert(dialect_plugins.end(), {"core", "mem"});

        std::vector<Dialect> dialects;
        thorin::Backends backends;
        thorin::Normalizers normalizers;
        if (!dialect_plugins.empty()) {
            for (const auto& dialect : dialect_plugins) {
                dialects.push_back(Dialect::load(dialect, dialect_paths));
                dialects.back().register_backends(backends);
                dialects.back().register_normalizers(normalizers);
            }
        }

        if (input.empty()) throw std::invalid_argument("error: no input given");
        if (input[0] == '-' || input.substr(0, 2) == "--")
            throw std::invalid_argument("error: unknown option " + input);

        World world;
        world.log.ostream = &std::cerr;
        world.log.level   = (Log::Level)verbose;
#if THORIN_ENABLE_CHECKS
        for (auto b : breakpoints) world.breakpoint(b);
#endif

        std::ifstream ifs(input);
        if (!ifs) {
            errln("error: cannot read file '{}'", input);
            return EXIT_FAILURE;
        }

        for (const auto& dialect : dialects)
            fe::Parser::import_module(world, dialect.name(), dialect_paths, &normalizers);

        fe::Parser parser(world, input, ifs, dialect_paths, &normalizers, os[Md]);
        parser.parse_module();

        if (os[H]) parser.bootstrap(*os[H]);

        PipelineBuilder builder;
        for (const auto& dialect : dialects) { dialect.register_passes(builder); }
        optimize(world, builder);

        if (os[Thorin]) *os[Thorin] << world << std::endl;
        if (os[Dot]) dot::emit(world, *os[Dot]);

        if (os[LL]) {
            if (auto it = backends.find("ll"); it != backends.end()) {
                it->second(world, *os[LL]);
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
