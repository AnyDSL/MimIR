#include <fstream>

#include <thorin/driver.h>

#include <thorin/fe/parser.h>
#include <thorin/pass/optimize.h>
#include <thorin/util/sys.h>

#include <thorin/plug/mem/mem.h>

using namespace thorin;
using namespace thorin::plug;

int main(int, char**) {
    try {
        Driver driver;
        auto& w = driver.world();
        driver.log().set(&std::cerr).set(Log::Level::Debug);

        auto parser = Parser(w);
        for (auto plugin : {"compile", "core"}) parser.plugin(plugin);

        // .Cn [%mem.M, I32, %mem.Ptr (I32, 0) .Cn [%mem.M, I32]]
        auto mem_t  = w.annex<mem::M>();
        auto argv_t = w.call<mem::Ptr0>(w.call<mem::Ptr0>(w.I32()));
        auto ret_t  = w.Cn({mem_t, w.I32()});
        auto main_t = w.Cn({mem_t, w.I32(), argv_t, ret_t});
        auto main   = w.mut_lam(main_t)->set("main");

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
