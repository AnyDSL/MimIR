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

enum Backends {
    Dot, H, LL, Md, Thorin, Num_Backends
};

static std::string be2str(size_t be) {
    switch (be) {
        case Dot:    return "dot"s;
        case H:      return "h"s;
        case LL:     return "ll"s;
        case Md:     return "md"s;
        case Thorin: return "thorin"s;
        default: unreachable();
    }
}

int main(int argc, char** argv) {
    try {
        static const auto version             = "thorin command-line utility version " THORIN_VER "\n";
        static constexpr const char* Backends = "dot|h|ll|md|thorin";

        bool show_help    = false;
        bool show_version = false;
        std::string input, prefix;
        std::string clang = sys::find_cmd("clang");
        std::vector<std::string> dialect_names, dialect_paths, emitters;
        std::vector<size_t> breakpoints;
        std::array<bool, Num_Backends> emit;
        emit.fill(false);
        std::array<std::string, Num_Backends> output;
        int verbose      = 0;
        auto inc_verbose = [&](bool) { ++verbose; };

        // clang-format off
        auto cli = lyra::cli()
            | lyra::help(show_help)
            | lyra::opt(show_version            )["-v"]["--version"      ]("Display version info and exit.")
            | lyra::opt(clang,         "clang"  )["-c"]["--clang"        ]("Path to clang executable (default: '" THORIN_WHICH " clang').")
            | lyra::opt(dialect_names, "dialect")["-d"]["--dialect"      ]("Dynamically load dialect [WIP].")
            | lyra::opt(dialect_paths, "path"   )["-D"]["--dialect-path" ]("Path to search dialects in.")
            | lyra::opt(emitters,      Backends )["-e"]["--emit"         ]("Select emitter. Multiple emitters can be specified simultaneously.").choices("dot", "h", "ll", "md", "thorin")
            | lyra::opt(inc_verbose             )["-V"]["--verbose"      ]("Verbose mode. Multiple -V options increase the verbosity. The maximum is 4.").cardinality(0, 4)
#if THORIN_ENABLE_CHECKS
            | lyra::opt(breakpoints,   "gid"    )["-b"]["--break"        ]("Trigger breakpoint upon construction of node with global id <gid>. Useful when running in a debugger.")
#endif
            | lyra::opt(prefix,        "prefix" )["-o"]["--output"       ]("Default prefix used for various output files.")
            | lyra::opt(output[Dot   ], "file"  )      ["--output-dot"   ]("Specify the dot output file.")
            | lyra::opt(output[H     ], "file"  )      ["--output-h"     ]("Specify the h output file.")
            | lyra::opt(output[LL    ], "file"  )      ["--output-ll"    ]("Specify the ll output file.")
            | lyra::opt(output[Md    ], "file"  )      ["--output-md"    ]("Specify the md output file.")
            | lyra::opt(output[Thorin], "file"  )      ["--output-thorin"]("Specify the thorin output file.")
            | lyra::arg(input,          "file"  )                         ("Input file.");
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

        for (const auto& e : emitters) {
            for (size_t be = 0; be != Num_Backends; ++be) {
                if (e == be2str(be)) {
                    emit[be] = true;
                    break;
                }
            }
        }

        // we always need core and mem, as long as we are not in bootstrap mode..
        if (!emit[H]) dialect_names.insert(dialect_names.end(), {"core", "mem"});

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

        std::array<std::ofstream, Num_Backends> ofs;
        std::array<std::ostream*, Num_Backends> os;


        for (size_t be = 0; be != Num_Backends; ++be) {
            if (!emit[be]) continue;
            if (output[be].empty()) output[be] = prefix + "."s + be2str(be);
            if (output[be] == "-") {
                os[be] = &std::cout;
            } else {
                ofs[be].open(output[be]);
                os[be] = &ofs[be];
            }
        }

        Parser parser(world, input, ifs, dialect_paths, &normalizers, emit[Md] ? os[Md] : nullptr);
        parser.parse_module();

        if (emit[H]) parser.bootstrap(*os[H]);

        PipelineBuilder builder;
        for (const auto& dialect : dialects) { dialect.register_passes(builder); }
        optimize(world, builder);

        if (emit[Thorin]) *os[Thorin] << world << std::endl;
        if (emit[Dot]) dot::emit(world, *os[Dot]);

        if (emit[LL]) {
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
