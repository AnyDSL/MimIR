#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <lyra/lyra.hpp>

#include "thorin/config.h"
#include "thorin/dialects.h"
#include "thorin/driver.h"

#include "thorin/be/dot/dot.h"
#include "thorin/fe/parser.h"
#include "thorin/pass/optimize.h"
#include "thorin/pass/pass.h"
#include "thorin/pass/pipelinebuilder.h"
#include "thorin/phase/phase.h"
#include "thorin/util/sys.h"

using namespace thorin;
using namespace std::literals;

enum Backends { Dot, H, LL, Md, Thorin, Num_Backends };

int main(int argc, char** argv) {
    try {
        static const auto version = "thorin command-line utility version " THORIN_VER "\n";

        Driver driver;
        bool show_help          = false;
        bool show_version       = false;
        bool list_dialect_paths = false;
        std::string input, prefix;
        std::string clang = sys::find_cmd("clang");
        std::vector<std::string> dialect_plugins, dialect_paths;
        std::vector<size_t> breakpoints;
        std::array<std::string, Num_Backends> output;
        int verbose      = 0;
        int opt          = 2;
        auto inc_verbose = [&](bool) { ++verbose; };
        auto& flags      = driver.flags();

        // clang-format off
        auto cli = lyra::cli()
            | lyra::help(show_help)
            | lyra::opt(show_version             )["-v"]["--version"           ]("Display version info and exit.")
            | lyra::opt(list_dialect_paths       )["-l"]["--list-dialect-paths"]("List search paths in order and exit.")
            | lyra::opt(clang,          "clang"  )["-c"]["--clang"             ]("Path to clang executable (default: '" THORIN_WHICH " clang').")
            | lyra::opt(dialect_plugins,"dialect")["-d"]["--dialect"           ]("Dynamically load dialect [WIP].")
            | lyra::opt(dialect_paths,  "path"   )["-D"]["--dialect-path"      ]("Path to search dialects in.")
            | lyra::opt(inc_verbose              )["-V"]["--verbose"           ]("Verbose mode. Multiple -V options increase the verbosity. The maximum is 4.").cardinality(0, 4)
            | lyra::opt(opt,            "level"  )["-O"]["--optimize"          ]("Optimization level (default: 2).")
            | lyra::opt(output[Dot   ], "file"   )      ["--output-dot"        ]("Emits the Thorin program as a graph using Graphviz' DOT language.")
            | lyra::opt(output[H     ], "file"   )      ["--output-h"          ]("Emits a header file to be used to interface with a dialect in C++.")
            | lyra::opt(output[LL    ], "file"   )      ["--output-ll"         ]("Compiles the Thorin program to LLVM.")
            | lyra::opt(output[Md    ], "file"   )      ["--output-md"         ]("Emits the input formatted as Markdown.")
            | lyra::opt(output[Thorin], "file"   )["-o"]["--output-thorin"     ]("Emits the Thorin program again.")
            | lyra::opt(flags.dump_gid, "level"  )      ["--dump-gid"          ]("Dumps gid of inline expressions as a comment in output if <level> > 0. Use a <level> of 2 to also emit the gid of trivial defs.")
            | lyra::opt(flags.dump_recursive     )      ["--dump-recursive"    ]("Dumps Thorin program with a simple recursive algorithm that is not readable again from Thorin but is less fragile and also works for broken Thorin programs.")
#if THORIN_ENABLE_CHECKS
            | lyra::opt(breakpoints,    "gid"    )["-b"]["--break"             ]("Trigger breakpoint upon construction of node with global id <gid>. Useful when running in a debugger.")
            | lyra::opt(flags.reeval_breakpoints )      ["--reeval-breakpoints"]("Triggers breakpoint even upon unfying a node that has already been built.")
            | lyra::opt(flags.trace_gids         )      ["--trace-gids"        ]("Output gids during World::unify/insert.")
#endif
            | lyra::arg(input,          "file"   )                              ("Input file.")
            ;
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

        for (auto&& path : dialect_paths) driver.add_search_path(path);

        if (list_dialect_paths) {
            for (auto&& path : driver.search_paths()) std::cout << path << std::endl;
            std::exit(EXIT_SUCCESS);
        }

        World& world = driver.world();
#if THORIN_ENABLE_CHECKS
        for (auto b : breakpoints) world.breakpoint(b);
#endif
        world.log().set(&std::cerr).set((Log::Level)verbose);

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
        if (!os[H]) dialect_plugins.insert(dialect_plugins.end(), {"core", "mem", "compile", "opt"});

        std::vector<Dialect> dialects;
        thorin::Passes passes;
        if (!dialect_plugins.empty()) {
            for (const auto& dialect : dialect_plugins) {
                dialects.push_back(driver.load(dialect));
                dialects.back().register_passes(passes);
            }
        }

        if (input.empty()) throw std::invalid_argument("error: no input given");
        if (input[0] == '-' || input.substr(0, 2) == "--")
            throw std::invalid_argument("error: unknown option " + input);

        std::ifstream ifs(input);
        if (!ifs) {
            errln("error: cannot read file '{}'", input);
            return EXIT_FAILURE;
        }

        for (const auto& dialect : dialects) fe::Parser::import_module(world, world.sym(dialect.name()));

        auto sym = world.sym(std::move(input));
        world.set(sym);
        fe::Parser parser(world, sym, ifs, os[Md]);
        parser.parse_module();

        if (os[H]) {
            parser.bootstrap(*os[H]);
            opt = std::min(opt, 1);
        }

        // clang-format off
        switch (opt) {
            case 0:                             break;
            case 1: Phase::run<Cleanup>(world); break;
            case 2: optimize(world, passes, dialects);   break;
            default: errln("error: illegal optimization level '{}'", opt);
        }
        // clang-format on

        if (os[Thorin]) world.dump(*os[Thorin]);
        if (os[Dot]) dot::emit(world, *os[Dot]);

        if (os[LL]) {
            if (auto it = driver.backends().find("ll"); it != driver.backends().end()) {
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
