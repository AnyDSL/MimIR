#include <cstdlib>
#include <cstring>

#include <fstream>
#include <iostream>
#include <stdexcept>

#include <lyra/lyra.hpp>

#include "thorin/config.h"
#include "thorin/driver.h"

#include "thorin/ast/parser.h"
#include "thorin/be/h/bootstrap.h"
#include "thorin/pass/optimize.h"
#include "thorin/pass/pass.h"
#include "thorin/pass/pipelinebuilder.h"
#include "thorin/phase/phase.h"
#include "thorin/util/sys.h"

using namespace thorin;
using namespace std::literals;

int main(int argc, char** argv) {
    enum Backends { AST, D, Dot, H, LL, Md, Thorin, Num_Backends };

    try {
        static const auto version = "thorin command-line utility version " THORIN_VER "\n";

        Driver driver;
        bool show_help         = false;
        bool show_version      = false;
        bool list_search_paths = false;
        bool dot_follow_types  = false;
        bool dot_all_annexes   = false;
        std::string input, prefix;
        std::string clang = sys::find_cmd("clang");
        std::vector<std::string> plugins, search_paths;
#ifdef THORIN_ENABLE_CHECKS
        std::vector<size_t> breakpoints;
#endif
        std::array<std::string, Num_Backends> output;
        int verbose      = 0;
        int opt          = 2;
        auto inc_verbose = [&](bool) { ++verbose; };
        auto& flags      = driver.flags();

        // clang-format off
        auto cli = lyra::cli()
            | lyra::help(show_help)
            | lyra::opt(show_version                          )["-v"]["--version"               ]("Display version info and exit.")
            | lyra::opt(list_search_paths                     )["-l"]["--list-search-paths"     ]("List search paths in order and exit.")
            | lyra::opt(clang,          "clang"               )["-c"]["--clang"                 ]("Path to clang executable (default: '" THORIN_WHICH " clang').")
            | lyra::opt(plugins,        "plugin"              )["-p"]["--plugin"                ]("Dynamically load plugin.")
            | lyra::opt(search_paths,   "path"                )["-P"]["--plugin-path"           ]("Path to search for plugins.")
            | lyra::opt(inc_verbose                           )["-V"]["--verbose"               ]("Verbose mode. Multiple -V options increase the verbosity. The maximum is 4.").cardinality(0, 4)
            | lyra::opt(opt,            "level"               )["-O"]["--optimize"              ]("Optimization level (default: 2).")
            | lyra::opt(output[AST   ], "file"                )      ["--output-ast"            ]("Directly emits AST represntation of input.")
            | lyra::opt(output[D     ], "file"                )      ["--output-d"              ]("Emits dependency file containing a rule suitable for 'make' describing the dependencies of the source file (requires --output-h).")
            | lyra::opt(output[Dot   ], "file"                )      ["--output-dot"            ]("Emits the Thorin program as a graph using Graphviz' DOT language.")
            | lyra::opt(output[H     ], "file"                )      ["--output-h"              ]("Emits a header file to be used to interface with a plugin in C++.")
            | lyra::opt(output[LL    ], "file"                )      ["--output-ll"             ]("Compiles the Thorin program to LLVM.")
            | lyra::opt(output[Md    ], "file"                )      ["--output-md"             ]("Emits the input formatted as Markdown.")
            | lyra::opt(output[Thorin], "file"                )["-o"]["--output-thorin"         ]("Emits the Thorin program again.")
            | lyra::opt(flags.ascii                           )["-a"]["--ascii"                 ]("Use ASCII alternatives in output instead of UTF-8.")
            | lyra::opt(flags.bootstrap                       )      ["--bootstrap"             ]("Puts thorin into \"bootstrap mode\". This means a '.plugin' directive has the same effect as an '.import' and will not load a library. In addition, no standard plugins will be loaded.")
            | lyra::opt(dot_follow_types                      )      ["--dot-follow-types"      ]("Follow type dependencies in DOT output.")
            | lyra::opt(dot_all_annexes                       )      ["--dot-all-annexes"       ]("Output all annexes - even if unused - in DOT output.")
            | lyra::opt(flags.dump_gid, "level"               )      ["--dump-gid"              ]("Dumps gid of inline expressions as a comment in output if <level> > 0. Use a <level> of 2 to also emit the gid of trivial defs.")
            | lyra::opt(flags.dump_recursive                  )      ["--dump-recursive"        ]("Dumps Thorin program with a simple recursive algorithm that is not readable again from Thorin but is less fragile and also works for broken Thorin programs.")
            | lyra::opt(flags.aggressive_lam_spec             )      ["--aggr-lam-spec"         ]("Overrides LamSpec behavior to follow recursive calls.")
            | lyra::opt(flags.scalerize_threshold, "threshold")      ["--scalerize-threshold"   ]("Thorin will not scalerize tuples/packs/sigmas/arrays with a number of elements greater than or equal this threshold.")
#ifdef THORIN_ENABLE_CHECKS
            | lyra::opt(breakpoints,    "gid"                 )["-b"]["--break"                 ]("*Triggers breakpoint upon construction of node with global id <gid>. Useful when running in a debugger.")
            | lyra::opt(flags.reeval_breakpoints              )      ["--reeval-breakpoints"    ]("*Triggers breakpoint even upon unfying a node that has already been built.")
            | lyra::opt(flags.break_on_alpha_unequal          )      ["--break-on-alpha-unequal"]("*Triggers breakpoint as soon as two expressions turn out to be not alpha-equivalent.")
            | lyra::opt(flags.break_on_error                  )      ["--break-on-error"        ]("*Triggers breakpoint on ELOG.")
            | lyra::opt(flags.break_on_warn                   )      ["--break-on-warn"         ]("*Triggers breakpoint on WLOG.")
            | lyra::opt(flags.trace_gids                      )      ["--trace-gids"            ]("*Output gids during World::unify/insert.")
#endif
            | lyra::arg(input,          "file"                )                                  ("Input file.")
            ;
        // clang-format on

        if (auto result = cli.parse({argc, argv}); !result) throw std::invalid_argument(result.message());

        if (show_help) {
            std::cout << cli << std::endl;
#ifdef THORIN_ENABLE_CHECKS
            std::cout << "*These are developer options only enabled, if 'THORIN_ENABLE_CHECKS' is ON." << std::endl;
#endif
            std::cout << "Use \"-\" as <file> to output to stdout." << std::endl;
            return EXIT_SUCCESS;
        }

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
#ifdef THORIN_ENABLE_CHECKS
        for (auto b : breakpoints) world.breakpoint(b);
#endif
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

        // we always need standard plugins, as long as we are not in bootstrap mode
        if (!flags.bootstrap) plugins.insert(plugins.end(), {"core", "mem", "compile", "opt"});

        if (!plugins.empty())
            for (const auto& plugin : plugins) driver.load(plugin);

        if (input.empty()) throw std::invalid_argument("error: no input given");
        if (input[0] == '-' || input.substr(0, 2) == "--")
            throw std::invalid_argument("error: unknown option " + input);

        auto path = fs::path(input);
        world.set(path.filename().replace_extension().string());
        auto parser = ast::Parser(world);
#if 0
        parser.import(driver.sym(input), os[Md]);

        if (auto dep = os[D]) {
            if (auto autogen_h = output[H]; !autogen_h.empty()) {
                *dep << autogen_h << ": ";
                assert(!driver.imports().empty());
                for (auto sep = ""; const auto& [path, _] : driver.imports() | std::views::drop(1)) {
                    *dep << sep << path;
                    sep = " \\\n ";
                }
            } else {
                throw std::invalid_argument("error: --output-d requires --output-h");
            }
            *dep << std::endl;
        }

        if (flags.bootstrap) {
            if (auto h = os[H])
                bootstrap(driver, world.sym(fs::path{path}.filename().replace_extension().string()), *h);
            opt = std::min(opt, 1);
        }

        switch (opt) {
            case 0: break;
            case 1: Phase::run<Cleanup>(world); break;
            case 2:
                parser.import("opt");
                optimize(world);
                break;
            default: error("illegal optimization level '{}'", opt);
        }
#endif

        if (os[AST]) {
            auto mod = parser.import(driver.sym(input), os[Md]);
            Tab tab;
            mod->stream(tab, *os[AST]);
        }
        if (os[Thorin]) world.dump(*os[Thorin]);
        if (os[Dot]) world.dot(*os[Dot], dot_all_annexes, dot_follow_types);

        if (os[LL]) {
            if (auto backend = driver.backend("ll"))
                backend(world, *os[LL]);
            else
                error("'ll' emitter not loaded; try loading 'mem' plugin");
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
