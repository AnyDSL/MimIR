#include <fstream>

#include <mim/driver.h>

#include <mim/ast/parser.h>
#include <mim/pass/optimize.h>
#include <mim/util/sys.h>

#include <mim/plug/mem/mem.h>

using namespace mim;
using namespace mim::plug;

int main(int, char**) {
    try {
        Driver driver;
        auto& w  = driver.world();
        auto ast = ast::AST(w);
        driver.log().set(&std::cerr).set(Log::Level::Debug);

        auto parser = ast::Parser(ast);
        for (auto plugin : {"compile", "core"}) parser.plugin(plugin);

        // Cn [%mem.M, I32, %mem.Ptr (I32, 0) Cn [%mem.M, I32]]
        auto mem_t  = w.annex<mem::M>();
        auto argv_t = w.call<mem::Ptr0>(w.call<mem::Ptr0>(w.type_i32()));
        auto main   = w.mut_fun({mem_t, w.type_i32(), argv_t}, {mem_t, w.type_i32()})->set("main");

        auto [mem, argc, argv, ret] = main->vars<4>();
        main->app(false, ret, {mem, argc});
        main->make_external();

        optimize(w);
        std::ofstream ofs("hello.ll");
        driver.backend("ll")(w, ofs);
        ofs.close(); // make sure everything is written before clang is invoked

        sys::system("clang hello.ll -o hello -Wno-override-module");
        outln("exit code: {}", sys::system("./hello a b c"));
    } catch (const std::exception& e) {
        errln("{}", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        errln("error: unknown exception");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
