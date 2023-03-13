#pragma once

#include "thorin/driver.h"

#include "thorin/be/h/bootstrapper.h"
#include "thorin/fe/ast.h"
#include "thorin/fe/lexer.h"
#include "thorin/fe/scopes.h"

namespace thorin::fe {

/// Parses Thorin code into the provided World.
///
/// The logic behind the various parse methods is as follows:
/// 1. The `parse_*` method does **not** have a `std::string_view ctxt` parameter:
///
///     It's the **caller's responsibility** to first make appropriate
///     [FIRST/FOLLOW](https://www.cs.uaf.edu/~cs331/notes/FirstFollow.pdf) checks.
///     Otherwise, an assertion will be triggered in the case of a syntax error.
///
/// 2. The `parse_*` method does have a `std::string_view ctxt` parameter:
///
///      The **called method** checks this and spits out an appropriate error message using `ctxt` in the case of a
///      syntax error.
///
/// 3. The `parse_*` method does have a `std::string_view ctxt = {}` parameter **with default argument**:
///
///      * If default argument is **elided** we have the same behavior as in 1.
///      * If default argument is **provided** we have the same behavior as in 2.
class Parser {
public:
    Parser(World&, Sym file, std::istream&, std::ostream* md = nullptr);

    World& world() { return lexer_.world(); }
    Driver& driver() { return world().driver(); }

    /// @name entry points
    ///@{
    static Parser import_module(World&, Sym);
    void parse_module();
    void bootstrap(std::ostream&);
    ///@}

private:
    const auto& normalizers() { return driver().normalizers(); }

    /// @name Tracker
    ///@{
    /// Trick to easily keep track of Loc%ations.
    class Tracker {
    public:
        Tracker(Parser& parser, const Pos& pos)
            : parser_(parser)
            , pos_(pos) {}

        Loc loc() const { return {parser_.prev_.file, pos_, parser_.prev_.finis}; }
        Dbg dbg(Sym sym) const { return {loc(), sym}; }

    private:
        Parser& parser_;
        Pos pos_;
    };

    /// @name misc parsing helpers
    ///@{
    template<class F>
    void parse_list(std::string ctxt, Tok::Tag delim_l, F f, Tok::Tag sep = Tok::Tag::T_comma) {
        expect(delim_l, ctxt);
        auto delim_r = Tok::delim_l2r(delim_l);
        if (!ahead().isa(delim_r)) {
            do { f(); } while (accept(sep) && !ahead().isa(delim_r));
        }
        expect(delim_r, std::string("closing delimiter of a ") + ctxt);
    }
    Dbg parse_sym(std::string_view ctxt = {});
    void parse_import();
    Ref parse_type_ascr(std::string_view ctxt);
    ///@}

    /// @name exprs
    ///@{
    Ref parse_expr(std::string_view ctxt, Tok::Prec = Tok::Prec::Bot);
    Ref parse_primary_expr(std::string_view ctxt);
    Ref parse_infix_expr(Tracker, const Def* lhs, Tok::Prec = Tok::Prec::Bot);
    Ref parse_extract(Tracker, const Def*, Tok::Prec);
    ///@}

    /// @name primary exprs
    ///@{
    Ref parse_Cn();
    Ref parse_arr();
    Ref parse_pack();
    Ref parse_block();
    Ref parse_sigma();
    Ref parse_tuple();
    Ref parse_type();
    Ref parse_pi();
    Ref parse_lit();
    Ref parse_var();
    Ref parse_insert();
    Lam* parse_lam(bool decl = false);
    ///@}

    /// @name ptrns
    ///@{

    /// Depending on @p tag, this parses a `()`-style (Tok::Tag::D_paren_l) or `[]`-style (Tok::Tag::D_brckt_l) Ptrn.
    std::unique_ptr<Ptrn> parse_ptrn(Tok::Tag tag, std::string_view ctxt, Tok::Prec = Tok::Prec::Bot);
    std::unique_ptr<TuplePtrn> parse_tuple_ptrn(Tracker, bool, Sym);
    ///@}

    /// @name decls
    ///@{
    Ref parse_decls(std::string_view ctxt);
    void parse_ax();
    void parse_let();
    void parse_nom();
    /// If @p sym is **not** empty, this is an inline definition of @p sym,
    /// otherwise it's a standalone definition.
    void parse_def(Dbg dbg = {});
    ///@}

    /// @name get next Tok and manage Location
    ///@{

    /// Factory method to build a Parser::Tracker.
    Tracker tracker() { return Tracker(*this, ahead().loc().begin); }

    /// Get lookahead.
    Tok ahead(size_t i = 0) const {
        assert(i < Max_Ahead);
        return ahead_[i];
    }

    /// Invoke Lexer to retrieve next Tok%en.
    Tok lex();

    /// If Parser::ahead() is a @p tag, Parser::lex(), and return `true`.
    std::optional<Tok> accept(Tok::Tag tag);

    /// Parser::lex Parser::ahead() which must be a @p tag.
    /// Issue err%or with @p ctxt otherwise.
    Tok expect(Tok::Tag tag, std::string_view ctxt);

    /// Consume Parser::ahead which must be a @p tag; asserts otherwise.
    Tok eat([[maybe_unused]] Tok::Tag tag) {
        assert(tag == ahead().tag() && "internal parser error");
        return lex();
    }
    ///@}

    /// @name error messages
    ///@{
    /// Issue an error message of the form:
    /// "expected \<what\>, got '\<tok>\' while parsing \<ctxt\>"
    [[noreturn]] void syntax_err(std::string_view what, const Tok& tok, std::string_view ctxt);

    /// Same above but uses @p ahead() as @p tok.
    [[noreturn]] void syntax_err(std::string_view what, std::string_view ctxt) { syntax_err(what, ahead(), ctxt); }
    ///@}

    Lexer lexer_;
    Scopes scopes_;
    Def2Fields def2fields_;
    Loc prev_;
    std::string dialect_;
    static constexpr size_t Max_Ahead = 2; ///< maximum lookahead
    std::array<Tok, Max_Ahead> ahead_;     ///< SLL look ahead
    SymSet imported_;
    h::Bootstrapper bootstrapper_;
    Sym anonymous_;
};

} // namespace thorin::fe
