#include <cstdlib>
#include <cstring>

#include <fstream>
#include <iostream>

#include <dlfcn.h>

#include "thorin/config.h"

#include "thorin/be/ll/ll.h"
#include "thorin/fe/parser.h"
#include "thorin/pass/pass.h"
#include "thorin/util/stream.h"

using namespace thorin;

static const auto usage = "Usage: thorin [options] file\n"
                          "\n"
                          "Options:\n"
                          "    -h, --help      display this help and exit\n"
                          "    -c, --clang     path to clang executable (default: {})\n"
                          "    -l, --emit-llvm emit LLVM\n"
                          "    -v, --version   display version info and exit\n"
                          "    -d, --dialect   dynamically load dialect [WIP]\n"
                          "\n"
                          "Hint: use '-' as file to read from stdin.\n";

static const auto version = "thorin command-line utility version " THORIN_VER "\n";

#ifdef _WIN32
#    define popen  _popen
#    define pclose _pclose
#endif

/// see https://stackoverflow.com/a/478960
std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) { throw std::runtime_error("popen() failed!"); }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) { result += buffer.data(); }
    return result;
}

void test_plugin(const char* name) {
    auto handle = dlopen(name, RTLD_LAZY);
    if (!handle) throw std::logic_error("cannot open plugin");

    auto create  = (CreateIPass)dlsym(handle, "create");
    auto destroy = (DestroyIPass)dlsym(handle, "destroy");

    if (!create || !destroy) throw std::logic_error("cannot find symbol");

    World world;
    PassMan man(world);
    auto pass = create(man);
    outln("hi from: '{}'", pass->name());
    destroy(pass);
}

int main(int argc, char** argv) {
    std::string clang;
    bool emit_llvm = false;

    try {
        const char* file = nullptr;
        clang            = exec("which clang");
        clang.erase(std::remove(clang.begin(), clang.end(), '\n'), clang.end());

        for (int i = 1; i != argc; ++i) {
            auto is_option = [&](auto o1, auto o2) { return strcmp(o1, argv[i]) == 0 || strcmp(o2, argv[i]) == 0; };

            auto check_last = [=](auto msg) {
                if (i + 1 == argc) throw std::logic_error(msg);
            };

            if (is_option("-c", "--clang")) {
                check_last("--clang option requires path to clang executable");
                clang = argv[++i];
            } else if (is_option("-l", "--emit--lvm")) {
                emit_llvm = true;
            } else if (is_option("-h", "--help")) {
                errf(usage, clang);
                return EXIT_SUCCESS;
            } else if (is_option("-d", "--dialect")) {
                check_last("--dialect requires name of dialect to load");
                test_plugin(argv[++i]);
                return EXIT_SUCCESS;
            } else if (is_option("-v", "--version")) {
                std::cerr << version;
                return EXIT_SUCCESS;
            } else if (file == nullptr) {
                file = argv[i];
            } else {
                throw std::logic_error("multiple input files given");
            }
        }

        if (file == nullptr) throw std::logic_error("no input file given");

        World world;
        if (strcmp("-", file) == 0) {
            Parser parser(world, "<stdin>", std::cin);
            // exp = parser.parse_prg();
        } else {
            std::ifstream ifs(file);
            Parser parser(world, file, ifs);
            // exp = parser.parse_prg();
        }

        // if (num_errors != 0) {
        // std::cerr << num_errors << " error(s) encountered" << std::endl;
        // return EXIT_FAILURE;
        //}

        // if (eval) exp = exp->eval();
        // exp->dump();

        if (emit_llvm) {
            std::ofstream of("test.ll");
            Stream s(of);
            thorin::ll::emit(world, s);
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
        errf(usage, clang);
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "error: unknown exception" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
