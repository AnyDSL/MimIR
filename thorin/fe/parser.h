#pragma once

#include "thorin/driver.h"

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
    Parser(World& world)
        : world_(world)
        , anonymous_(world.sym("_"))
        , return_(world.sym("return")) {}

    World& world() { return world_; }
    Driver& driver() { return world().driver(); }
    void import(fs::path, std::ostream* md = nullptr);
    void import(std::istream&, const fs::path* = nullptr, std::ostream* md = nullptr);
    void plugin(fs::path);
    const Scopes& scopes() const { return scopes_; }

private:
    /// @name Tracker
    ///@{
    /// Trick to easily keep track of Loc%ations.
    class Tracker {
    public:
        Tracker(Parser& parser, const Pos& pos)
            : parser_(parser)
            , pos_(pos) {}

        Loc loc() const { return {parser_.prev().path, pos_, parser_.prev().finis}; }
        Dbg dbg(Sym sym) const { return {loc(), sym}; }

    private:
        Parser& parser_;
        Pos pos_;
    };

    Loc& prev() { return state_.prev; }

    /// Factory method to build a Parser::Tracker.
    Tracker tracker() { return Tracker(*this, ahead().loc().begin); }
    ///@}

    /// @name get next token
    ///@{
    /// Get lookahead.
    Tok& ahead(size_t i = 0) {
        assert(i < Max_Ahead);
        return state_.ahead[i];
    }

    Lexer& lexer() { return lexers_.top(); }
    bool main() const { return lexers_.size() == 1; }

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

    /// @name parse misc
    ///@{
    void parse_module();
    Dbg parse_id(std::string_view ctxt = {});
    std::pair<Annex&, bool> parse_annex(std::string_view ctxt = {});
    std::pair<Dbg, bool> parse_name(std::string_view ctxt = {});
    void parse_import();
    void parse_plugin();
    Ref parse_type_ascr(std::string_view ctxt = {});
    void register_annex(Dbg, Ref);

    template<class F> void parse_list(std::string ctxt, Tok::Tag delim_l, F f, Tok::Tag sep = Tok::Tag::T_comma) {
        expect(delim_l, ctxt);
        auto delim_r = Tok::delim_l2r(delim_l);
        if (!ahead().isa(delim_r)) {
            do { f(); } while (accept(sep) && !ahead().isa(delim_r));
        }
        expect(delim_r, std::string("closing delimiter of a ") + ctxt);
    }
    ///@}

    /// @name parse exprs
    ///@{
    Ref parse_expr(std::string_view ctxt, Tok::Prec = Tok::Prec::Bot);
    Ref parse_bin_expr(std::string_view ctxt, Tok::Prec = Tok::Prec::Bot);
    Ref parse_primary_expr(std::string_view ctxt);
    Ref parse_infix_expr(Tracker, const Def* lhs, Tok::Prec = Tok::Prec::Bot);
    Ref parse_extract_expr(Tracker, const Def*, Tok::Prec);
    ///@}

    /// @name parse primary exprs
    ///@{
    Ref parse_arr_expr();
    Ref parse_pack_expr();
    Ref parse_block_expr();
    Ref parse_sigma_expr();
    Ref parse_tuple_expr();
    Ref parse_type_expr();
    Pi* parse_pi_expr(Pi* = nullptr);
    Ref parse_lit_expr();
    Ref parse_insert_expr();
    Ref parse_ret_expr();
    Lam* parse_lam(bool decl = false);
    ///@}

    /// @name parse ptrns
    ///@{

    /// Depending on @p tag, this parses a `()`-style (Tok::Tag::D_paren_l) or `[]`-style (Tok::Tag::D_brckt_l) Ptrn.
    std::unique_ptr<Ptrn> parse_ptrn(Tok::Tag tag, std::string_view ctxt, Tok::Prec = Tok::Prec::Bot);
    std::unique_ptr<TuplePtrn> parse_tuple_ptrn(Tracker, bool rebind, Sym, Def* = nullptr);
    ///@}

    /// @name parse decls
    ///@{

    /// If @p ctxt ...
    /// * ... empty: **Only** decls are parsed. @returns `nullptr`
    /// * ... **non**-empty: Decls are parsed, then an expression. @returns expression.
    Ref parse_decls(std::string_view ctxt);
    void parse_ax_decl();
    void parse_let_decl();
    void parse_sigma_decl();
    void parse_pi_decl();
    ///@}

    /// @name error messages
    ///@{
    /// Issue an error message of the form:
    /// "expected \<what\>, got '\<tok>\' while parsing \<ctxt\>"
    [[noreturn]] void syntax_err(std::string_view what, const Tok& tok, std::string_view ctxt);

    /// Same above but uses @p ahead() as @p tok.
    [[noreturn]] void syntax_err(std::string_view what, std::string_view ctxt) { syntax_err(what, ahead(), ctxt); }
    ///@}

    static constexpr size_t Max_Ahead = 2; ///< maximum lookahead
    using Ahead                       = std::array<Tok, Max_Ahead>;

    World& world_;
    std::stack<Lexer> lexers_;
    struct {
        Loc prev;
        Ahead ahead; ///< SLL look ahead
    } state_;
    Scopes scopes_;
    Def2Fields def2fields_;
    Sym anonymous_;
    Sym return_;
};

} // namespace thorin::fe
