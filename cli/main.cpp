#include <cstdlib>
#include <cstring>

#include <fstream>
#include <iostream>
#include <CLI/CLI.hpp>

#include "thorin/config.h"
#include "thorin/driver.h"

#include "thorin/be/dot/dot.h"
#include "thorin/be/h/bootstrap.h"
#include "thorin/fe/parser.h"
#include "thorin/pass/optimize.h"
#include "thorin/pass/pass.h"
#include "thorin/pass/pipelinebuilder.h"
#include "thorin/phase/phase.h"
#include "thorin/util/sys.h"
#include "CLI/Error.hpp"

using namespace thorin;
using namespace std::literals;

enum Backends { Dot, H, LL, Md, Thorin, Num_Backends };

int main(int argc, char** argv) {
    CLI::App app{"The Higher ORder INtermediate Representation"};
    auto formatter = app.get_formatter();
    formatter->label("INT", "<level>");
    formatter->label("TEXT", "<arg>");
    try {
        static const auto version = "thorin command-line utility version " THORIN_VER "\n";

        Driver driver;
        bool show_version      = false;
        bool list_search_paths = false;
        std::string input, prefix;
        std::string clang = sys::find_cmd("clang");
        std::vector<std::string> plugins, search_paths;
        std::vector<size_t> breakpoints;
        std::array<std::string, Num_Backends> output;
        int verbose      = 0;
        int opt          = 2;
        auto& flags      = driver.flags();

        // clang-format off
        app.add_option(      "input file",      input,                      "Input file.");
        app.add_flag  ("-v,--version",          show_version,               "Display version info and exit.");
        app.add_flag  ("-V,--verbose",          verbose,                    "Verbose mode. Multiple -V options increase the verbosity. The maximum is 4.");
        app.add_flag  ("-l,--list-search_paths",list_search_paths,          "List search paths in order and exit.");
        app.add_flag  (   "--bootstrap",        flags.bootstrap,            "Puts thorin into \"bootstrap mode\". This means a `.plugin` directive has the same effect as an `.import` and will not load a library.");
        app.add_option("-c,--clang",            clang,                      "Path to clang executable.")->default_str(clang);
        app.add_option("-p,--plugin",           plugins,                    "Dynamically load plugin.");
        app.add_option("-P,--plugin-path",      search_paths,               "Path to search for plugins.");
        app.add_option(   "--output-dot",       output[Dot   ],             "Emits the Thorin program as a graph using Graphviz' DOT language.");
        app.add_option(   "--output-h",         output[H     ],             "Emits a header file to be used to interface with a plugin in C++.");
        app.add_option(   "--output-ll",        output[LL    ],             "Compiles the Thorin program to LLVM.");
        app.add_option(   "--output-md",        output[Md    ],             "Emits the input formatted as Markdown.");
        app.add_option("-o,--output-thorin",    output[Thorin],             "Emits the Thorin program again.");
        app.add_option("-O,--optimize",         opt,                        "Optimization level.")->default_val(2);
#if THORIN_ENABLE_CHECKS
        app.add_option("-b,--break",            breakpoints,                "Trigger breakpoint upon construction of node with global id <gid>. Useful when running in a debugger.");
        app.add_flag  ("--reeval-breakpoints",  flags.reeval_breakpoints,   "Triggers breakpoint even upon unfying a node that has already been built.");
        app.add_flag  ("--trace-gids",          flags.trace_gids,           "Output gids during World::unify/insert.");
#endif
        app.add_option("--dump-gid",            flags.dump_gid,             "Dumps gid of inline expressions as a comment in output if <level> > 0. Use a <level> of 2 to also emit the gid of trivial defs.")->default_val(0);
        app.add_flag  ("--dump-recursive",      flags.dump_recursive,       "Dumps Thorin program with a simple recursive algorithm that is not readable again from Thorin but is less fragile and also works for broken Thorin programs.");
        app.parse(argc, argv);
        // clang-format on

        if (show_version) {
            std::cerr << version;
            std::exit(EXIT_SUCCESS);
        }

        for (auto&& path : search_paths) driver.add_search_path(path);

        if (list_search_paths) {
            for (auto&& path : driver.search_paths() | std::views::drop(1)) // skip first empty path
                std::cout << path << std::endl;
            std::exit(EXIT_SUCCESS);
        }

        World& world = driver.world();
#if THORIN_ENABLE_CHECKS
        for (auto b : breakpoints) world.breakpoint(b);
#endif
        if (verbose > int(Log::Level::Debug)) throw CLI::ValidationError("--verbose can only be given 0 to 4 times");
        driver.log().set(&std::cerr).set((Log::Level)verbose);

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

        // we always need core and mem, as long as we are not in bootstrap mode
        if (!flags.bootstrap) plugins.insert(plugins.end(), {"core", "mem", "compile", "opt"});

        if (!plugins.empty())
            for (const auto& plugin : plugins) driver.load(plugin);

        if (input.empty()) throw std::invalid_argument("error: no input given");
        if (input[0] == '-' || input.substr(0, 2) == "--")
            throw std::invalid_argument("error: unknown option " + input);

        auto path = fs::path(input);
        world.set(path.filename().replace_extension().string());
        auto parser = fe::Parser(world);
        parser.import(input, os[Md]);

        if (auto h = os[H]) {
            bootstrap(driver, world.sym(fs::path{path}.filename().replace_extension().string()), *h);
            opt = std::min(opt, 1);
        } else {
            parser.import("opt");
        }

        // clang-format off
        switch (opt) {
            case 0:                             break;
            case 1: Phase::run<Cleanup>(world); break;
            case 2: optimize(world);            break;
            default: error("illegal optimization level '{}'", opt);
        }
        // clang-format on

        if (os[Thorin]) world.dump(*os[Thorin]);
        if (os[Dot]) dot::emit(world, *os[Dot]);

        if (os[LL]) {
            if (auto backend = driver.backend("ll"))
                backend(world, *os[LL]);
            else
                error("'ll' emitter not loaded; try loading 'mem' plugin");
        }
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    } catch (const std::exception& e) {
        errln("{}", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        errln("error: unknown exception");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
