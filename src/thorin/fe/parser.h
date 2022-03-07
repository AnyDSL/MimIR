#ifndef THORIN_FE_PARSER_H
#define THORIN_FE_PARSER_H

#include "thorin/world.h"

#include "thorin/fe/lexer.h"

namespace thorin {

class THORIN_API Parser {
public:
    Parser(World&, std::string_view, std::istream&);
    World& world() { return lexer_.world(); }

private:
    Sym parse_sym(std::string_view ctxt);
    const Def* parse_def(std::string_view ctxt, Tok::Prec);
    const Def* parse_primary_def(std::string_view ctxt);
    const Def* parse_primary_def();
    const Def* parse_extract();

    /// Trick to easily keep track of @p Loc%ations.
    class Tracker {
    public:
        Tracker(Parser& parser, const Pos& pos)
            : parser_(parser)
            , pos_(pos) {}

        operator Loc() const { return {parser_.prev_.file, pos_, parser_.prev_.finis}; }

    private:
        Parser& parser_;
        Pos pos_;
    };

    /// Factory method to build a @p Tracker.
    Tracker tracker() { return Tracker(*this, ahead().loc().begin); }
    const Def* dbg(Tracker t) { return world().dbg((Loc)t); }

    /// Invoke @p Lexer to retrieve next @p Tok%en.
    Tok lex();

    /// Get lookahead.
    Tok ahead() const { return ahead_; }

    /// If @p ahead() is a @p tag, @p lex(), and return @c true.
    bool accept(Tok::Tag tag);

    /// @p lex @p ahead() which must be a @p tag.
    /// Issue @p err%or with @p ctxt otherwise.
    bool expect(Tok::Tag tag, std::string_view ctxt);

    /// Consume @p ahead which must be a @p tag; @c asserts otherwise.
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
