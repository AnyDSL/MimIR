#include <cstdio>

#include <fstream>
#include <ranges>
#include <sstream>

#include "thorin/driver.h"
#include "thorin/rewrite.h"

#include "thorin/fe/parser.h"

#include "dialects/core/core.h"
#include "helpers.h"

using namespace thorin;

TEST(Zip, fold) {
    Driver driver;
    World& w    = driver.world();
    auto parser = fe::Parser(w);

    std::istringstream iss(".plugin core;"
                           ".let _32 = 4294967296;"
                           ".let I32 = .Idx _32;"
                           ".let a = ((0:I32, 1:I32,  2:I32), ( 3:I32,  4:I32,  5:I32));"
                           ".let b = ((6:I32, 7:I32,  8:I32), ( 9:I32, 10:I32, 11:I32));"
                           ".let c = ((6:I32, 8:I32, 10:I32), (12:I32, 14:I32, 16:I32));"
                           ".let r = %core.zip (2, (2, 3)) (2, (I32, I32), 1, I32, %core.wrap.add 0) (a, b);");
    parser.import(iss);
    auto c = parser.scopes().find({Loc(), driver.sym("c")});
    auto r = parser.scopes().find({Loc(), driver.sym("r")});

    EXPECT_TRUE(r->is_term());
    EXPECT_TRUE(!r->type()->is_term());
    EXPECT_EQ(c, r);
}

TEST(World, simplify_one_tuple) {
    Driver driver;
    World& w = driver.world();

    ASSERT_EQ(w.lit_ff(), w.tuple({w.lit_ff()})) << "constant fold (false) -> false";

    auto type = w.nom_sigma(w.type(), 2);
    type->set(Defs{w.type_nat(), w.type_nat()});
    ASSERT_EQ(type, w.sigma({type})) << "constant fold [nom] -> nom";

    auto v = w.tuple(type, {w.lit_idx(42), w.lit_idx(1337)});
    ASSERT_EQ(v, w.tuple({v})) << "constant fold ({42, 1337}) -> {42, 1337}";
}

TEST(World, dependent_extract) {
    Driver driver;
    World& w = driver.world();

    auto sig = w.nom_sigma(w.type<1>(), 2); // sig = [T: *, T]
    sig->set(0, w.type<0>());
    sig->set(1, sig->var(0_u64));
    auto a = w.axiom(sig);
    ASSERT_EQ(a->proj(2, 1)->type(), a->proj(2, 0_u64)); // type_of(a#1_2) == a#0_1
}

TEST(Axiom, mangle) {
    Driver driver;
    World& w = driver.world();

    EXPECT_EQ(Axiom::demangle(w, *Axiom::mangle(w.sym("test"))), w.sym("test"));
    EXPECT_EQ(Axiom::demangle(w, *Axiom::mangle(w.sym("azAZ09_"))), w.sym("azAZ09_"));
    EXPECT_EQ(Axiom::demangle(w, *Axiom::mangle(w.sym("01234567"))), w.sym("01234567"));
    EXPECT_FALSE(Axiom::mangle(w.sym("012345678")));
    EXPECT_FALSE(Axiom::mangle(w.sym("!")));
    // Check whether lower 16 bits are properly ignored
    EXPECT_EQ(Axiom::demangle(w, *Axiom::mangle(w.sym("test")) | 0xFF_u64), w.sym("test"));
    EXPECT_EQ(Axiom::demangle(w, *Axiom::mangle(w.sym("01234567")) | 0xFF_u64), w.sym("01234567"));
}

TEST(Axiom, split) {
    Driver driver;
    World& w = driver.world();

    auto [plugin, group, tag] = Axiom::split(w, w.sym("%foo.bar.baz"));
    EXPECT_EQ(plugin, w.sym("foo"));
    EXPECT_EQ(group, w.sym("bar"));
    EXPECT_EQ(tag, w.sym("baz"));
}

TEST(trait, idx) {
    Driver driver;
    World& w    = driver.world();
    auto parser = fe::Parser(w);
    parser.plugin("core");

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

Ref normalize_test_curry(Ref type, Ref callee, Ref arg) {
    auto& w = arg->world();
    return w.raw_app(type, callee, w.lit_nat(42));
}

TEST(Axiom, curry) {
    Driver driver;
    World& w = driver.world();

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

        auto ax = w.axiom(normalize_test_curry, curry, trip, pi)->set("test_5_3");
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

        auto ax = w.axiom(normalize_test_curry, curry, trip, rec)->set("test_1_1");
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

        auto ax = w.axiom(normalize_test_curry, 3, 0, pi)->set("test_3_0");
        auto a1 = w.app(w.app(w.app(ax, n[0]), n[1]), n[2]);
        auto a2 = w.app(a1, n[3]);

        EXPECT_EQ(a1->as<App>()->curry(), 0);
        EXPECT_EQ(a2->as<App>()->curry(), Axiom::Trip_End);

        std::ostringstream os;
        a2->stream(os, 0);
        EXPECT_EQ(os.str(), "%test_3_0 0 1 42 3\n");
    }
}

TEST(Type, Level) {
    Driver driver;
    World& w = driver.world();
    auto pi  = w.pi(w.type<7>(), w.type<2>());
    EXPECT_EQ(as_lit(pi->type()->isa<Type>()->level()), 8);
}
