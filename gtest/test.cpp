#include <cstdio>

#include <fstream>
#include <ranges>
#include <sstream>

#include "thorin/driver.h"
#include "thorin/rewrite.h"

#include "thorin/fe/parser.h"
#include "thorin/plug/core/core.h"

#include "helpers.h"

using namespace thorin;
using namespace thorin::plug;

TEST(Zip, fold) {
    Driver driver;
    World& w    = driver.world();
    auto parser = Parser(w);

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

    auto type = w.mut_sigma(w.type(), 2);
    type->set(Defs{w.type_nat(), w.type_nat()});
    ASSERT_EQ(type, w.sigma({type})) << "constant fold [mut] -> mut";

    auto v = w.tuple(type, {w.lit_idx(42), w.lit_idx(1337)});
    ASSERT_EQ(v, w.tuple({v})) << "constant fold ({42, 1337}) -> {42, 1337}";
}

TEST(World, dependent_extract) {
    Driver driver;
    World& w = driver.world();

    auto sig = w.mut_sigma(w.type<1>(), 2); // sig = [T: *, T]
    sig->set(0, w.type<0>());
    sig->set(1, sig->var(0_u64));
    auto a = w.axiom(sig);
    ASSERT_EQ(a->proj(2, 1)->type(), a->proj(2, 0_u64)); // type_of(a#1_2) == a#0_1
}

TEST(Annex, mangle) {
    Driver driver;
    World& w = driver.world();

    EXPECT_EQ(Annex::demangle(w, *Annex::mangle(w.sym("test"))), w.sym("test"));
    EXPECT_EQ(Annex::demangle(w, *Annex::mangle(w.sym("azAZ09_"))), w.sym("azAZ09_"));
    EXPECT_EQ(Annex::demangle(w, *Annex::mangle(w.sym("01234567"))), w.sym("01234567"));
    EXPECT_FALSE(Annex::mangle(w.sym("012345678")));
    EXPECT_FALSE(Annex::mangle(w.sym("!")));
    // Check whether lower 16 bits are properly ignored
    EXPECT_EQ(Annex::demangle(w, *Annex::mangle(w.sym("test")) | 0xFF_u64), w.sym("test"));
    EXPECT_EQ(Annex::demangle(w, *Annex::mangle(w.sym("01234567")) | 0xFF_u64), w.sym("01234567"));
}

TEST(Annex, split) {
    Driver driver;
    World& w = driver.world();

    auto [plugin, group, tag] = Annex::split(w, w.sym("%foo.bar.baz"));
    EXPECT_EQ(plugin, w.sym("foo"));
    EXPECT_EQ(group, w.sym("bar"));
    EXPECT_EQ(tag, w.sym("baz"));
}

TEST(trait, idx) {
    Driver driver;
    World& w    = driver.world();
    auto parser = Parser(w);
    parser.plugin("core");

    EXPECT_EQ(Lit::as(op(core::trait::size, w.type_idx(0x0000'0000'0000'00FF_n))), 1);
    EXPECT_EQ(Lit::as(op(core::trait::size, w.type_idx(0x0000'0000'0000'0100_n))), 1);
    EXPECT_EQ(Lit::as(op(core::trait::size, w.type_idx(0x0000'0000'0000'0101_n))), 2);

    EXPECT_EQ(Lit::as(op(core::trait::size, w.type_idx(0x0000'0000'0000'FFFF_n))), 2);
    EXPECT_EQ(Lit::as(op(core::trait::size, w.type_idx(0x0000'0000'0001'0000_n))), 2);
    EXPECT_EQ(Lit::as(op(core::trait::size, w.type_idx(0x0000'0000'0001'0001_n))), 4);

    EXPECT_EQ(Lit::as(op(core::trait::size, w.type_idx(0x0000'0000'FFFF'FFFF_n))), 4);
    EXPECT_EQ(Lit::as(op(core::trait::size, w.type_idx(0x0000'0001'0000'0000_n))), 4);
    EXPECT_EQ(Lit::as(op(core::trait::size, w.type_idx(0x0000'0001'0000'0001_n))), 8);

    EXPECT_EQ(Lit::as(op(core::trait::size, w.type_idx(0xFFFF'FFFF'FFFF'FFFF_n))), 8);
    EXPECT_EQ(Lit::as(op(core::trait::size, w.type_idx(0x0000'0000'0000'0000_n))), 8);
}

Ref normalize_test_curry(Ref type, Ref callee, Ref arg) {
    auto& w = arg->world();
    return w.raw_app(type, callee, w.lit_nat(42));
}

TEST(Axiom, curry) {
    Driver driver;
    World& w = driver.world();

    auto n   = DefVec(11, [&w](size_t i) { return w.lit_nat(i); });
    auto nat = w.type_nat();

    {
        // N -> N -> N -> N -> N
        //           ^         |
        //           |         |
        //           +---------+
        auto rec = w.mut_pi(w.type())->set_dom(nat);
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
        auto rec = w.mut_pi(w.type())->set_dom(nat);
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
    EXPECT_EQ(Lit::as(pi->type()->isa<Type>()->level()), 8);
}

TEST(Check, alpha) {
    Driver driver;
    World& w = driver.world();
    auto pi  = w.pi(w.type_nat(), w.type_nat());

    // λx.x
    auto lxx = w.mut_lam(pi);
    lxx->set(false, lxx->var());
    // λy.y
    auto lyy = w.mut_lam(pi);
    lyy->set(false, lyy->var());
    // λz.x
    auto lzx = w.mut_lam(pi);
    lzx->set(false, lxx->var());
    // λw.y
    auto lwy = w.mut_lam(pi);
    lwy->set(false, lyy->var());
    // λ_.x
    auto l_x = w.lam(pi, false, lxx->var());
    // λ_.y
    auto l_y = w.lam(pi, false, lyy->var());
    // λ_.0
    auto l_0 = w.lam(pi, false, w.lit_nat_0());
    // λ_.1
    auto l_1 = w.lam(pi, false, w.lit_nat_1());

    auto check = [](Ref l1, Ref l2, bool infer_res, bool non_infer_res) {
        EXPECT_EQ(Check::alpha<true>(l1, l2), infer_res);
        EXPECT_EQ(Check::alpha<true>(l2, l1), infer_res);
        EXPECT_EQ(Check::alpha<false>(l1, l2), non_infer_res);
        EXPECT_EQ(Check::alpha<false>(l2, l1), non_infer_res);
    };

    check(lxx, lxx, true, true);
    check(lxx, lyy, true, true);
    check(lxx, lzx, false, false);
    check(lxx, lwy, false, false);
    check(lxx, l_x, false, false);
    check(lxx, l_y, false, false);
    check(lxx, l_0, false, false);
    check(lxx, l_1, false, false);

    check(lyy, lyy, true, true);
    check(lyy, lzx, false, false);
    check(lyy, lwy, false, false);
    check(lyy, l_x, false, false);
    check(lyy, l_y, false, false);
    check(lyy, l_0, false, false);
    check(lyy, l_1, false, false);

    check(lzx, lzx, true, true);
    check(lzx, lwy, true, false);
    check(lzx, l_x, true, true);
    check(lzx, l_y, true, false);
    check(lzx, l_0, false, false);
    check(lzx, l_1, false, false);

    check(lwy, lwy, true, true);
    check(lwy, l_x, true, false);
    check(lwy, l_y, true, true);
    check(lwy, l_0, false, false);
    check(lwy, l_1, false, false);

    check(l_x, l_x, true, true);
    check(l_x, l_y, true, false);
    check(l_x, l_0, false, false);
    check(l_x, l_1, false, false);

    check(l_y, l_y, true, true);
    check(l_y, l_0, false, false);
    check(l_y, l_1, false, false);

    check(l_0, l_0, true, true);
    check(l_0, l_1, false, false);

    check(l_1, l_1, true, true);
}

TEST(ADT, Span) {
    {
        int a[3]        = {0, 1, 2};
        auto s          = Span(a);
        auto& [x, y, z] = s;
        y               = 23;
        EXPECT_EQ(a[1], 23);
    }

    int a[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    int* p   = a;

    auto check = [](auto span, int b, int e) {
        EXPECT_EQ(span.front(), b);
        EXPECT_EQ(span.back(), e - 1);
        EXPECT_EQ(span.size(), e - b);
    };

    {
        auto vec         = std::vector(a, a + 9);
        const auto& cvec = vec;
        auto vs          = Span(vec);
        auto vv          = View<int>(vec);
        auto vw          = Span(cvec);
        static_assert(std::is_same_v<decltype(vs), Span<int, std::dynamic_extent>>, "incorrect constness");
        static_assert(std::is_same_v<decltype(vv), Span<const int, std::dynamic_extent>>, "incorrect constness");
        static_assert(std::is_same_v<decltype(vw), Span<const int, std::dynamic_extent>>, "incorrect constness");
    }

    auto s_0_9 = Span(a);
    auto d_0_9 = Span(p, 9);
    static_assert(std::is_same_v<decltype(s_0_9), Span<int, 9>>, "dynamic_extent broken");
    static_assert(std::is_same_v<decltype(d_0_9), Span<int, std::dynamic_extent>>, "dynamic_extent broken");
    check(s_0_9, 0, 9);
    check(d_0_9, 0, 9);

    {
        auto s_7_9 = s_0_9.subspan<7>();
        auto s_2_6 = s_0_9.subspan<2, 4>();
        auto s_1_4 = d_0_9.subspan<1, 3>();
        auto d_2_9 = d_0_9.subspan<2>();
        auto d_7_9 = s_0_9.subspan(7);
        auto d_2_6 = s_0_9.subspan(2, 4);
        static_assert(std::is_same_v<decltype(s_7_9), Span<int, 2>>, "dynamic_extent broken");
        static_assert(std::is_same_v<decltype(s_2_6), Span<int, 4>>, "dynamic_extent broken");
        static_assert(std::is_same_v<decltype(s_1_4), Span<int, 3>>, "dynamic_extent broken");
        static_assert(std::is_same_v<decltype(d_2_9), Span<int, std::dynamic_extent>>, "dynamic_extent broken");
        static_assert(std::is_same_v<decltype(d_7_9), Span<int, std::dynamic_extent>>, "dynamic_extent broken");
        static_assert(std::is_same_v<decltype(d_2_6), Span<int, std::dynamic_extent>>, "dynamic_extent broken");

        check(s_7_9, 7, 9);
        check(s_2_6, 2, 6);
        check(s_1_4, 1, 4);
        check(d_2_9, 2, 9);
        check(d_7_9, 7, 9);
        check(d_2_6, 2, 6);
    }
    {
        auto s_0_2 = s_0_9.rsubspan<7>();
        auto s_3_7 = s_0_9.rsubspan<2, 4>();
        auto s_5_8 = d_0_9.rsubspan<1, 3>();
        auto d_0_6 = d_0_9.rsubspan<3>();
        auto d_0_2 = s_0_9.rsubspan(7);
        auto d_3_7 = s_0_9.rsubspan(2, 4);
        static_assert(std::is_same_v<decltype(s_0_2), Span<int, 2>>, "dynamic_extent broken");
        static_assert(std::is_same_v<decltype(s_3_7), Span<int, 4>>, "dynamic_extent broken");
        static_assert(std::is_same_v<decltype(s_5_8), Span<int, 3>>, "dynamic_extent broken");
        static_assert(std::is_same_v<decltype(d_0_6), Span<int, std::dynamic_extent>>, "dynamic_extent broken");
        static_assert(std::is_same_v<decltype(d_0_2), Span<int, std::dynamic_extent>>, "dynamic_extent broken");
        static_assert(std::is_same_v<decltype(d_3_7), Span<int, std::dynamic_extent>>, "dynamic_extent broken");

        check(s_0_2, 0, 2);
        check(s_3_7, 3, 7);
        check(s_5_8, 5, 8);
        check(d_0_6, 0, 6);
        check(d_0_2, 0, 2);
        check(d_3_7, 3, 7);
    }
}
