#include <fstream>

#include <dialects/mem/mem.h>
#include <thorin/driver.h>
#include <thorin/fe/parser.h>
#include <thorin/pass/optimize.h>
#include <thorin/util/sys.h>

using namespace thorin;

int main(int, char**) {
    try {
        Driver driver;
        auto& world = driver.world();
        driver.log().set(&std::cerr).set(Log::Level::Debug);

        auto parser = fe::Parser(world);
        for (auto plugin : {"compile", "core"}) parser.plugin(plugin);

        // .Cn [%mem.M, I32, %mem.Ptr (I32, 0) .Cn [%mem.M, I32]]
        auto mem_t  = mem::type_mem(world);
        auto i32_t  = world.type_int(32);
        auto argv_t = mem::type_ptr(mem::type_ptr(i32_t));
        auto ret_t  = world.cn({mem_t, i32_t});
        auto main_t = world.cn({mem_t, i32_t, argv_t, ret_t});
        auto main   = world.mut_lam(main_t)->set("main");

        auto [mem, argc, argv, ret] = main->vars<4>();
        main->app(false, ret, {mem, argc});
        main->make_external();

        optimize(world);
        std::ofstream ofs("hello.ll");
        driver.backend("ll")(world, ofs);
        ofs.close(); // make sure everything is written before clang is invoked

        sys::system("clang hello.ll -o hello -Wno-override-module");
        assert(4 == WEXITSTATUS(sys::system("./hello a b c")));
    } catch (const std::exception& e) {
        errln("{}", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        errln("error: unknown exception");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
