#include <cstdio>

#include <fstream>
#include <ranges>

#include "thorin/error.h"
#include "thorin/world.h"

// #include "thorin/be/ll/ll.h"
#include "thorin/util/sys.h"

#include "helpers.h"

using namespace thorin;

TEST(Zip, fold) {
    World w;

    // clang-format off
    auto a = w.tuple({w.tuple({w.lit_idx( 0), w.lit_idx( 1), w.lit_idx( 2)}),
                      w.tuple({w.lit_idx( 3), w.lit_idx( 4), w.lit_idx( 5)})});

    auto b = w.tuple({w.tuple({w.lit_idx( 6), w.lit_idx( 7), w.lit_idx( 8)}),
                      w.tuple({w.lit_idx( 9), w.lit_idx(10), w.lit_idx(11)})});

    auto c = w.tuple({w.tuple({w.lit_idx( 6), w.lit_idx( 8), w.lit_idx(10)}),
                      w.tuple({w.lit_idx(12), w.lit_idx(14), w.lit_idx(16)})});

    auto f = w.fn(Wrap::add, w.lit_nat(0), w.lit_nat(bitwidth2size(32)));
    auto i32_t = w.type_int_(32);
    auto res = w.app(w.app(w.app(w.ax_zip(), {/*r*/w.lit_nat(2), /*s*/w.tuple({w.lit_nat(2), w.lit_nat(3)})}),
                                             {/*n_i*/ w.lit_nat(2), /*Is*/w.pack(2, i32_t), /*n_o*/w.lit_nat(1), /*Os*/i32_t, f}),
                                             {a, b});
    // clang-format on

    res->dump(0);
    EXPECT_EQ(c, res);
}

TEST(World, simplify_one_tuple) {
    World w;

    ASSERT_EQ(w.lit_ff(), w.tuple({w.lit_ff()})) << "constant fold (false) -> false";

    auto type = w.nom_sigma(w.type(), 2);
    type->set({w.type_nat(), w.type_nat()});
    ASSERT_EQ(type, w.sigma({type})) << "constant fold [nom] -> nom";

    auto v = w.tuple(type, {w.lit_idx(42), w.lit_idx(1337)});
    ASSERT_EQ(v, w.tuple({v})) << "constant fold ({42, 1337}) -> {42, 1337}";
}

TEST(World, dependent_extract) {
    World w;
    auto sig = w.nom_sigma(w.type<1>(), 2); // sig = [T: *, T]
    sig->set(0, w.type<0>());
    sig->set(1, sig->var(0_u64));
    auto a = w.axiom(sig);
    ASSERT_EQ(a->proj(2, 1)->type(), a->proj(2, 0_u64)); // type_of(a#1_2) == a#0_1
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

TEST(Axiom, split) {
    auto [dialect, group, tag] = *Axiom::split("%foo.bar.baz");
    EXPECT_EQ(dialect, "foo");
    EXPECT_EQ(group, "bar");
    EXPECT_EQ(tag, "baz");
}
