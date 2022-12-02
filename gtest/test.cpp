#include <cstdio>

#include <fstream>
#include <ranges>

#include "thorin/error.h"
#include "thorin/world.h"

#include "thorin/fe/parser.h"
#include "thorin/util/sys.h"

#include "dialects/core/core.h"
#include "helpers.h"

using namespace thorin;

TEST(Zip, fold) {
    World w;

    Normalizers normalizers;
    auto core_d = Dialect::load("core", {});
    core_d.register_normalizers(normalizers);
    fe::Parser::import_module(w, "core", {}, &normalizers);

    // clang-format off
    auto a = w.tuple({w.tuple({w.lit_idx( 0), w.lit_idx( 1), w.lit_idx( 2)}),
                      w.tuple({w.lit_idx( 3), w.lit_idx( 4), w.lit_idx( 5)})});

    auto b = w.tuple({w.tuple({w.lit_idx( 6), w.lit_idx( 7), w.lit_idx( 8)}),
                      w.tuple({w.lit_idx( 9), w.lit_idx(10), w.lit_idx(11)})});

    auto c = w.tuple({w.tuple({w.lit_idx( 6), w.lit_idx( 8), w.lit_idx(10)}),
                      w.tuple({w.lit_idx(12), w.lit_idx(14), w.lit_idx(16)})});

    auto f = core::fn(core::wrap::add, w.lit_nat(Idx::bitwidth2size(32)), 0_n);
    auto i32_t = w.type_int(32);
    auto res = w.app(w.app(w.app(w.ax<core::zip>(), {/*r*/w.lit_nat(2), /*s*/w.tuple({w.lit_nat(2), w.lit_nat(3)})}),
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

TEST(trait, idx) {
    World w;
    Normalizers normalizers;
    auto core_d = Dialect::load("core", {});
    core_d.register_normalizers(normalizers);
    fe::Parser::import_module(w, "core", {}, &normalizers);

    EXPECT_EQ(as_lit(op(core::trait::size, w.type_idx(0x0000'0000'0000'00FF_n))), 1);
    EXPECT_EQ(as_lit(op(core::trait::size, w.type_idx(0x0000'0000'0000'0100_n))), 1);
    EXPECT_EQ(as_lit(op(core::trait::size, w.type_idx(0x0000'0000'0000'0101_n))), 2);

    EXPECT_EQ(as_lit(op(core::trait::size, w.type_idx(0x0000'0000'0000'FFFF_n))), 2);
    EXPECT_EQ(as_lit(op(core::trait::size, w.type_idx(0x0000'0000'0001'0000_n))), 2);
    EXPECT_EQ(as_lit(op(core::trait::size, w.type_idx(0x0000'0000'0001'0001_n))), 4);

    EXPECT_EQ(as_lit(op(core::trait::size, w.type_idx(0x0000'0000'FFFF'FFFF_n))), 4);
    EXPECT_EQ(as_lit(op(core::trait::size, w.type_idx(0x0000'0001'0000'0000_n))), 4);
    EXPECT_EQ(as_lit(op(core::trait::size, w.type_idx(0x0000'0001'0000'0001_n))), 8);

    EXPECT_EQ(as_lit(op(core::trait::size, w.type_idx(0xFFFF'FFFF'FFFF'FFFF_n))), 8);
    EXPECT_EQ(as_lit(op(core::trait::size, w.type_idx(0x0000'0000'0000'0000_n))), 8);
}

const Def* normalize_test_curry(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& w = arg->world();
    return w.raw_app(type, callee, w.lit_nat(42), dbg);
}

TEST(Axiom, curry) {
    World w;

    DefArray n(11, [&w](size_t i) { return w.lit_nat(i); });
    auto nat = w.type_nat();

    {
        // N -> N -> N -> N -> N
        //           ^         |
        //           |         |
        //           +---------+
        auto rec = w.nom_pi(w.type())->set_dom(nat);
        rec->set_codom(w.pi(nat, w.pi(nat, rec)));
        auto pi = w.pi(nat, w.pi(nat, rec));

        auto [curry, trip] = Axiom::infer_curry_and_trip(pi);
        EXPECT_EQ(curry, 5);
        EXPECT_EQ(trip, 3);

        auto ax = w.axiom(normalize_test_curry, curry, trip, pi, w.dbg("test_5_3"));
        auto a1 = w.app(w.app(w.app(w.app(w.app(ax, n[0]), n[1]), n[2]), n[3]), n[4]);
        auto a2 = w.app(w.app(w.app(a1, n[5]), n[6]), n[7]);
        auto a3 = w.app(w.app(w.app(a2, n[8]), n[9]), n[10]);

        EXPECT_EQ(a1->as<App>()->curry(), 0);
        EXPECT_EQ(a2->as<App>()->curry(), 0);
        EXPECT_EQ(a3->as<App>()->curry(), 0);

        std::ostringstream os;
        a3->stream(os, 0);
        EXPECT_EQ(os.str(), "%test_5_3 0 1 2 3 42 5 6 42 8 9 42\n");
    }
    {
        auto rec = w.nom_pi(w.type())->set_dom(nat);
        rec->set_codom(rec);

        auto [curry, trip] = Axiom::infer_curry_and_trip(rec);
        EXPECT_EQ(curry, 1);
        EXPECT_EQ(trip, 1);

        auto ax = w.axiom(normalize_test_curry, curry, trip, rec, w.dbg("test_1_1"));
        auto a1 = w.app(ax, n[0]);
        auto a2 = w.app(a1, n[1]);
        auto a3 = w.app(a2, n[2]);

        EXPECT_EQ(a1->as<App>()->curry(), 0);
        EXPECT_EQ(a2->as<App>()->curry(), 0);
        EXPECT_EQ(a3->as<App>()->curry(), 0);

        std::ostringstream os;
        a3->stream(os, 0);
        EXPECT_EQ(os.str(), "%test_1_1 42 42 42\n");
    }
    {
        auto pi            = w.pi(nat, w.pi(nat, w.pi(nat, w.pi(nat, nat))));
        auto [curry, trip] = Axiom::infer_curry_and_trip(pi);
        EXPECT_EQ(curry, 4);
        EXPECT_EQ(trip, 0);

        auto ax = w.axiom(normalize_test_curry, 3, 0, pi, w.dbg("test_3_0"));
        auto a1 = w.app(w.app(w.app(ax, n[0]), n[1]), n[2]);
        auto a2 = w.app(a1, n[3]);

        EXPECT_EQ(a1->as<App>()->curry(), 0);
        EXPECT_EQ(a2->as<App>()->curry(), Axiom::Trip_End);

        std::ostringstream os;
        a2->stream(os, 0);
        EXPECT_EQ(os.str(), "%test_3_0 0 1 42 3\n");
    }
}
