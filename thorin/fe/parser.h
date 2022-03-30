#ifndef THORIN_FE_PARSER_H
#define THORIN_FE_PARSER_H

#include "thorin/world.h"

#include "thorin/fe/lexer.h"

namespace thorin {

/// Parses Thorin code into the provided World.
///
/// The behind the various parse methods is as follows:
/// 1. The `parse_*` method does **not** have a `std::string_view ctxt` parameter:
///
///     It's the **caller's responsibility** to first make appropriate
///     [FIRST/FOLLOW](https://www.cs.uaf.edu/~cs331/notes/FirstFollow.pdf) checks.
///     Otherwise, an assertion will be triggered in the case of a syntax error.
/// 2. The `parse_*` method does have a `std::string_view ctxt` parameter:
///
///      The **called method** checks this and spits out an appropriate error message using `ctxt` in the case of a
///      syntax error.
/// 3. The `parse_* method does have a `std::string_view ctxt = {}` parameter **with default argument**:
///
///      * If default argument is **elided** we have the same behavior as in 1.
///      * If default argument is **provided** we have the same behavior as in 2.
class Parser {
public:
    Parser(World&, std::string_view, std::istream&);

    World& world() { return lexer_.world(); }
    void parse_module();

private:
    Sym parse_sym(std::string_view ctxt = {});
    const Def* parse_def(std::string_view ctxt, Tok::Prec = Tok::Prec::Bottom);
    const Def* parse_primary_def(std::string_view ctxt);
    const Def* parse_extract();

    /// @name primary defs
    ///@{
    const Def* parse_pack_or_array(bool pack);
    const Def* parse_block();
    const Def* parse_sigma();
    const Def* parse_tuple();
    ///@}

    template<class F>
    auto parse_list(Tok::Tag delim_r, F f, Tok::Tag sep = Tok::Tag::P_comma) {
        DefVec result;
        if (!ahead().isa(delim_r)) {
            do { result.emplace_back(f()); } while (accept(sep) && !ahead().isa(delim_r));
        }
        return result;
    }

    template<class F>
    auto parse_list(const char* ctxt, Tok::Tag delim_l, F f, Tok::Tag sep = Tok::Tag::P_comma) {
        eat(delim_l);
        auto delim_r = Tok::delim_l2r(delim_l);
        auto result  = parse_list(delim_r, f, sep);
        expect(delim_r, ctxt);
        return result;
    }

    /// Trick to easily keep track of Loc%ations.
    class Tracker {
    public:
        Tracker(Parser& parser, const Pos& pos)
            : parser_(parser)
            , pos_(pos) {}

        operator const Def*() const {
            return parser_.world().dbg(Debug("", Loc(parser_.prev_.file, pos_, parser_.prev_.finis)));
        }

    private:
        Parser& parser_;
        Pos pos_;
    };

    /// Factory method to build a Parser::Tracker.
    Tracker tracker() { return Tracker(*this, ahead().loc().begin); }
    const Def* dbg(Tracker t) { return world().dbg((Loc)t); }

    /// Invoke Lexer to retrieve next Tok%en.
    Tok lex();

    /// Get lookahead.
    Tok ahead() const { return ahead_; }

    /// If Parser::ahead() is a @p tag, Parser::lex(), and return `true`.
    bool accept(Tok::Tag tag);

    /// Parser::lex Parser::ahead() which must be a @p tag.
    /// Issue err%or with @p ctxt otherwise.
    bool expect(Tok::Tag tag, std::string_view ctxt);

    /// Consume Parser::ahead which must be a @p tag; asserts otherwise.
    Tok eat([[maybe_unused]] Tok::Tag tag) {
        assert(tag == ahead().tag() && "internal parser error");
        return lex();
    }

    /// Issue an error message of the form:
    /// "expected \<what\>, got '\<tok>\' while parsing \<ctxt\>"
    void err(std::string_view what, const Tok& tok, std::string_view ctxt);

    /// Same above but uses @p ahead() as @p tok.
    void err(std::string_view what, std::string_view ctxt) { err(what, ahead(), ctxt); }

    Lexer lexer_;
    Loc prev_;
    Tok ahead_;
};

} // namespace thorin

#endif
