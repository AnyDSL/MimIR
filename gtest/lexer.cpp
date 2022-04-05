#include "thorin/fe/lexer.h"

#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "thorin/world.h"

using namespace std::literals;
using namespace thorin;

TEST(Lexer, Toks) {
    World world;
    std::istringstream is("{ } ( ) [ ] ‹ › « » : , . .lam .Pi λ Π");
    Lexer lexer(world, "<is>", is);

    EXPECT_TRUE(lexer.lex().isa(Tok::Tag::D_brace_l));
    EXPECT_TRUE(lexer.lex().isa(Tok::Tag::D_brace_r));
    EXPECT_TRUE(lexer.lex().isa(Tok::Tag::D_paren_l));
    EXPECT_TRUE(lexer.lex().isa(Tok::Tag::D_paren_r));
    EXPECT_TRUE(lexer.lex().isa(Tok::Tag::D_bracket_l));
    EXPECT_TRUE(lexer.lex().isa(Tok::Tag::D_bracket_r));
    EXPECT_TRUE(lexer.lex().isa(Tok::Tag::D_angle_l));
    EXPECT_TRUE(lexer.lex().isa(Tok::Tag::D_angle_r));
    EXPECT_TRUE(lexer.lex().isa(Tok::Tag::D_quote_l));
    EXPECT_TRUE(lexer.lex().isa(Tok::Tag::D_quote_r));
    EXPECT_TRUE(lexer.lex().isa(Tok::Tag::T_colon));
    EXPECT_TRUE(lexer.lex().isa(Tok::Tag::T_comma));
    EXPECT_TRUE(lexer.lex().isa(Tok::Tag::T_dot));
    EXPECT_TRUE(lexer.lex().isa(Tok::Tag::T_lam));
    EXPECT_TRUE(lexer.lex().isa(Tok::Tag::T_Pi));
    EXPECT_TRUE(lexer.lex().isa(Tok::Tag::T_lam));
    EXPECT_TRUE(lexer.lex().isa(Tok::Tag::T_Pi));
    EXPECT_TRUE(lexer.lex().isa(Tok::Tag::M_eof));
}

TEST(Lexer, Loc) {
    World world;
    std::istringstream is(" test  abc    def if  \nwhile λ foo   ");
    Lexer lexer(world, "<is>", is);
    auto t1 = lexer.lex();
    auto t2 = lexer.lex();
    auto t3 = lexer.lex();
    auto t4 = lexer.lex();
    auto t5 = lexer.lex();
    auto t6 = lexer.lex();
    auto t7 = lexer.lex();
    auto t8 = lexer.lex();
    StringStream s;
    s.fmt("{} {} {} {} {} {} {} {}", t1, t2, t3, t4, t5, t6, t7, t8);
    EXPECT_EQ(s.str(), "test abc def if while λ foo <eof>");
    // clang-format off
    EXPECT_EQ(t1.loc(), Loc("<is>", {1,  2}, {1,  5}));
    EXPECT_EQ(t2.loc(), Loc("<is>", {1,  8}, {1, 10}));
    EXPECT_EQ(t3.loc(), Loc("<is>", {1, 15}, {1, 17}));
    EXPECT_EQ(t4.loc(), Loc("<is>", {1, 19}, {1, 20}));
    EXPECT_EQ(t5.loc(), Loc("<is>", {2,  1}, {2,  5}));
    EXPECT_EQ(t6.loc(), Loc("<is>", {2,  7}, {2,  7}));
    EXPECT_EQ(t7.loc(), Loc("<is>", {2,  9}, {2, 11}));
    EXPECT_EQ(t8.loc(), Loc("<is>", {2, 14}, {2, 14}));
    // clang-format on
}

TEST(Lexer, Errors) {
    // TODO catch error

    World world;
    std::istringstream is1("asdf \xc0\xc0");
    Lexer l1(world, "<is1>", is1);
    l1.lex();
    l1.lex();

    std::istringstream is2("foo \xaa");
    Lexer l2(world, "<is2>", is2);
    l2.lex();
    l2.lex();

    // stray +/-
}

TEST(Lexer, Eof) {
    World world;
    std::istringstream is("");

    Lexer lexer(world, "<is>", is);
    for (int i = 0; i < 10; i++) EXPECT_TRUE(lexer.lex().isa(Tok::Tag::M_eof));
}

class Real : public testing::TestWithParam<int> {};

TEST_P(Real, sign) {
    World w;

    // clang-format off
    auto check = [&w](std::string s, r64 r) {
        const auto sign = GetParam();
        switch (sign) {
            case 0: break;
            case 1: s.insert(0, "+"sv); break;
            case 2: s.insert(0, "-"sv); break;
            default: unreachable();
        }

        std::istringstream is(s);
        Lexer lexer(w, "<is>", is);

        auto tag = lexer.lex();
        EXPECT_TRUE(tag.isa(Tok::Tag::L_r));
        EXPECT_EQ(std::bit_cast<r64>(tag.u()), sign == 2 ? -r : r);
    };

    check(  "2e+3",   2e+3); check(  "2E3",   2E3);
    check( "2.e-3",  2.e-3); check( "2.E3",  2.E3); check( "2.3",  2.3);
    check( ".2e+3",  .2e+3); check( ".2E3",  .2E3); check( ".23",  .23);
    check("2.3e-4", 2.3e-4); check("2.3E4", 2.3E4); check("2.34", 2.34);

    check(  "0x2p+3",   0x2p+3); check(  "0x2P3",   0x2P3);
    check( "0x2.p-3",  0x2.p-3); check( "0x2.P3",  0x2.P3);
    check( "0x.2p+3",  0x.2p+3); check( "0x.2P3",  0x.2P3);
    check("0x2.3p-4", 0x2.3p-4); check("0x2.3P4", 0x2.3P4);
    // clang-format on

    // TODO check for error: 0x2.34
    // TODO check for error: 2.34e
}

INSTANTIATE_TEST_SUITE_P(Lexer, Real, testing::Range(0, 3));
