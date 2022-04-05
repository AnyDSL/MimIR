#include <gtest/gtest.h>

#include "thorin/error.h"
#include "thorin/world.h"

#include "thorin/be/ll/ll.h"

using namespace thorin;

TEST(Zip, fold) {
    World w;

    // clang-format off
    auto a = w.tuple({w.tuple({w.lit_int( 0), w.lit_int( 1), w.lit_int( 2)}),
                      w.tuple({w.lit_int( 3), w.lit_int( 4), w.lit_int( 5)})});

    auto b = w.tuple({w.tuple({w.lit_int( 6), w.lit_int( 7), w.lit_int( 8)}),
                      w.tuple({w.lit_int( 9), w.lit_int(10), w.lit_int(11)})});

    auto c = w.tuple({w.tuple({w.lit_int( 6), w.lit_int( 8), w.lit_int(10)}),
                      w.tuple({w.lit_int(12), w.lit_int(14), w.lit_int(16)})});

    auto f = w.fn(Wrap::add, w.lit_nat(0), w.lit_nat(width2mod(32)));
    auto i32_t = w.type_int_width(32);
    auto res = w.app(w.app(w.app(w.ax_zip(), {/*r*/w.lit_nat(2), /*s*/w.tuple({w.lit_nat(2), w.lit_nat(3)})}),
                                             {/*n_i*/ w.lit_nat(2), /*Is*/w.pack(2, i32_t), /*n_o*/w.lit_nat(1), /*Os*/i32_t, f}),
                                             {a, b});
    // clang-format on

    res->dump(0);
    EXPECT_EQ(c, res);
}

TEST(Error, app) {
    World w;
    auto i32_t = w.type_int_width(32);
    auto r32_t = w.type_real(32);
    auto a     = w.axiom(w.cn({i32_t, r32_t}));
    auto i     = w.lit_int_width(32, 23);
    auto r     = w.lit_real(32, 23);
    w.app(a, {i, r}); // Ok!
    EXPECT_THROW(w.app(a, {r, i}), TypeError);
}

TEST(Main, ll) {
    World w;
    auto mem_t  = w.type_mem();
    auto i32_t  = w.type_int_width(32);
    auto argv_t = w.type_ptr(w.type_ptr(i32_t));

    // Cn [mem, i32, Cn [mem, i32]]
    auto main_t                 = w.cn({mem_t, i32_t, argv_t, w.cn({mem_t, i32_t})});
    auto main                   = w.nom_lam(main_t, w.dbg("main"));
    auto [mem, argc, argv, ret] = main->vars<4>();
    main->app(false, ret, {mem, argc});
    main->make_external();

    std::ofstream file("test.ll");
    Stream s(file);
    ll::emit(w, s);
    file.close();

#ifndef _MSC_VER
    // TODO make sure that proper clang is in path on Windows
    std::system("clang test.ll -o test -Wno-override-module");
    EXPECT_EQ(4, WEXITSTATUS(std::system("./test a b c")));
    EXPECT_EQ(7, WEXITSTATUS(std::system("./test a b c d e f")));
#endif
}

TEST(Axiom, mangle) {
    EXPECT_EQ(Axiom::demangle(*Axiom::mangle("test")), "test");
    EXPECT_EQ(Axiom::demangle(*Axiom::mangle("azAZ09_")), "azAZ09_");
    EXPECT_EQ(Axiom::demangle(*Axiom::mangle("01234567")), "01234567");
    EXPECT_FALSE(Axiom::mangle("012345678"));
    EXPECT_FALSE(Axiom::mangle("!"));
    // Check whether lower 16 bits are properly ignored
    EXPECT_EQ(Axiom::demangle(*Axiom::mangle("test") | 0xFF_u64), "test");
    EXPECT_EQ(Axiom::demangle(*Axiom::mangle("01234567") | 0xFF_u64), "01234567");
}

TEST(Main, loop) {
    World w;
    auto mem_t  = w.type_mem();
    auto i32_t  = w.type_int_width(32);
    auto argv_t = w.type_ptr(w.type_ptr(i32_t));
    auto i32_w  = w.lit_nat(width2mod(32));

    // Cn [mem, i32, i32**, Cn [mem, i32]]
    auto main_t                 = w.cn({mem_t, i32_t, argv_t, w.cn({mem_t, i32_t}, w.dbg("return"))});
    auto main                   = w.nom_lam(main_t, w.dbg("main"));
    auto [mem, argc, argv, ret] = main->vars<4>();

    auto body_t = w.cn(mem_t);

    auto lt     = w.fn(ICmp::ul, i32_w);
    auto loop_t = w.cn({mem_t, i32_t, i32_t});
    auto loop   = w.nom_lam(loop_t, w.dbg("loop"));

    auto body = w.nom_lam(body_t, w.dbg("body"));
    auto exit = w.nom_lam(body_t, w.dbg("exit"));

    {
        auto [lMem, iterVar, accumulator] = loop->vars<3>();
        loop->app(false, w.select(body, exit, w.app(lt, {iterVar, argc})), lMem);
    }

    auto add = w.fn(Wrap::add, w.lit_nat(0), i32_w);
    {
        auto [lMem, iterVar, accumulator] = loop->vars<3>();

        auto accumAdd = w.app(add, {iterVar, accumulator});
        auto iterInc  = w.app(add, {iterVar, w.lit_int(1)});
        body->app(false, loop, {lMem, iterInc, accumAdd});
    }
    {
        auto [lMem, iterVar, accumulator] = loop->vars<3>();
        exit->app(false, main->var(3), {main->var(0, nullptr), accumulator});
    }

    main->app(false, loop, {mem, w.lit_int(0), w.lit_int(0)});
    main->make_external();

    std::ofstream file("test.ll");
    Stream s(file);
    thorin::ll::emit(w, s);
    file.close();

    // TODO make sure that proper clang is in path on Windows
#ifndef _MSC_VER
    int status = std::system("clang test.ll -o `pwd`/test -Wno-override-module");
    EXPECT_EQ(0, WEXITSTATUS(status));
    status = std::system("./test a b c");
    EXPECT_EQ(6, WEXITSTATUS(status));
    status = std::system("./test a b c d");
    EXPECT_EQ(10, WEXITSTATUS(status));
#endif
}
