#pragma once

#include <filesystem>

#include "thorin/dialects.h"
#include "thorin/world.h"

#include "thorin/be/h/bootstrapper.h"
#include "thorin/fe/ast.h"
#include "thorin/fe/binder.h"
#include "thorin/fe/lexer.h"

namespace thorin {

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
    Parser(World&,
           std::string_view,
           std::istream&,
           ArrayRef<std::string>,
           const Normalizers*,
           std::ostream* md = nullptr);

    World& world() { return lexer_.world(); }

    /// @name entry points
    ///@{
    static Parser
    import_module(World&, std::string_view, ArrayRef<std::string> = {}, const Normalizers* normalizers = nullptr);
    void parse_module();
    void bootstrap(std::ostream&);
    ///@}

private:
    /// @name Tracker
    ///@{
    /// Trick to easily keep track of Loc%ations.
    class Tracker {
    public:
        Tracker(Parser& parser, const Pos& pos)
            : parser_(parser)
            , pos_(pos) {}

        Loc loc() const { return {parser_.prev_.file, pos_, parser_.prev_.finis}; }
        operator const Def*() const { return parser_.world().dbg({"", loc()}); }
        const Def* meta(const Def* m) const { return parser_.world().dbg({"", loc(), m}); }
        const Def* named(Sym sym) const { return parser_.world().dbg({sym.to_string(), loc()}); }
        const Def* named(const std::string& str) const { return parser_.world().dbg({str, loc()}); }

    private:
        Parser& parser_;
        Pos pos_;
    };

    Sym parse_sym(std::string_view ctxt = {});
    void parse_import();

    /// @name exprs
    ///@{
    const Def* parse_dep_expr(std::string_view ctxt, Binders*, Tok::Prec = Tok::Prec::Bot);
    const Def* parse_expr(std::string_view c, Tok::Prec p = Tok::Prec::Bot) { return parse_dep_expr(c, nullptr, p); }
    const Def* parse_primary_expr(std::string_view ctxt, Binders*);
    const Def* parse_extract(Tracker, const Def*, Tok::Prec);
    ///@}

    /// @name primary exprs
    ///@{
    const Def* parse_Cn(Binders*);
    const Def* parse_arr();
    const Def* parse_pack();
    const Def* parse_block();
    const Def* parse_sigma(Binders*);
    const Def* parse_tuple();
    const Def* parse_type();
    const Def* parse_pi(Binders*);
    const Def* parse_lam();
    const Def* parse_lit();
    const Def* parse_var();
    const Def* parse_insert();
    ///@}

    /// @name ptrns
    ///@{
    std::unique_ptr<Ptrn> parse_ptrn(std::string_view ctxt);
    std::unique_ptr<IdPtrn> parse_id_ptrn();
    std::unique_ptr<TuplePtrn> parse_tuple_ptrn();
    ///@}

    /// @name decls
    ///@{
    const Def* parse_decls(bool expr = true);
    void parse_ax();
    void parse_let();
    void parse_nom();
    /// If @p sym is **not** empty, this is an inline definition of @p sym,
    /// otherwise it's a standalone definition.
    void parse_def(Sym sym = {});
    ///@}

    template<class F>
    void parse_list(std::string ctxt, Tok::Tag delim_l, F f, Tok::Tag sep = Tok::Tag::T_comma) {
        expect(delim_l, ctxt);
        auto delim_r = Tok::delim_l2r(delim_l);
        if (!ahead().isa(delim_r)) {
            do { f(); } while (accept(sep) && !ahead().isa(delim_r));
        }
        expect(delim_r, std::string("closing delimiter of a ") + ctxt);
    }

    void parse_var_list(Binders&);

    /// Factory method to build a Parser::Tracker.
    Tracker tracker() { return Tracker(*this, ahead().loc().begin); }
    const Def* dbg(Tracker t) { return world().dbg((Loc)t); }
    ///@}

    /// @name get next Tok
    ///@{
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
    template<class... Args>
    [[noreturn]] void err(Loc loc, const char* fmt, Args&&... args) {
        thorin::err<ParseError>(loc, fmt, std::forward<Args&&>(args)...);
    }

    /// Issue an error message of the form:
    /// "expected \<what\>, got '\<tok>\' while parsing \<ctxt\>"
    [[noreturn]] void err(std::string_view what, const Tok& tok, std::string_view ctxt);

    /// Same above but uses @p ahead() as @p tok.
    [[noreturn]] void err(std::string_view what, std::string_view ctxt) { err(what, ahead(), ctxt); }
    ///@}

    Parser(World&,
           std::string_view,
           std::istream&,
           ArrayRef<std::string>,
           const Normalizers*,
           const Binder&,
           const SymSet&);

    Lexer lexer_;
    Binder binder_;
    Loc prev_;
    std::string dialect_;
    static constexpr size_t Max_Ahead = 2; ///< maximum lookahead
    std::array<Tok, Max_Ahead> ahead_;     ///< SLL look ahead
    SymSet imported_;
    h::Bootstrapper bootstrapper_;
    std::vector<std::string> user_search_paths_;
    const Normalizers* normalizers_;
};

} // namespace thorin
