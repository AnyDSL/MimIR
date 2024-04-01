#pragma once

#include <fe/parser.h>

#include "thorin/ast/ast.h"
#include "thorin/ast/lexer.h"

namespace thorin::ast {

constexpr size_t Look_Ahead = 2;

/// Parses Thorin code as AST.
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
class Parser : public fe::Parser<Tok, Tok::Tag, Look_Ahead, Parser> {
public:
    Parser(AST& ast)
        : ast_(ast) {}

    AST& ast() { return ast_; }
    Driver& driver() { return ast().driver(); }
    Ptr<Module> import(std::string_view sv) { return import(driver().sym(sv)); }
    Ptr<Module> import(Sym, std::ostream* md = nullptr);
    Ptr<Module> import(std::istream&, const fs::path* = nullptr, std::ostream* md = nullptr);
    void plugin(Sym);
    void plugin(const char* name) { return plugin(driver().sym(name)); }

private:
    template<class T, class... Args> auto ptr(Args&&... args) {
        return ast_.ptr<const T>(std::forward<Args&&>(args)...);
    }

    Dbg dbg(const Tracker& tracker, Sym sym) const { return {tracker.loc(), sym}; }
    Lexer& lexer() { return *lexer_; }

    /// @name parse misc
    ///@{
    Ptr<Module> parse_module();
    Dbg parse_id(std::string_view ctxt = {});
    std::pair<Annex&, bool> parse_annex(std::string_view ctxt = {});
    Dbg parse_name(std::string_view ctxt = {});
    void parse_import();
    void parse_plugin();
    Ptr<Expr> parse_type_ascr(std::string_view ctxt = {});

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
    Ptr<Expr> parse_expr(std::string_view ctxt, Tok::Prec = Tok::Prec::Bot);
    Ptr<Expr> parse_primary_expr(std::string_view ctxt);
    Ptr<Expr> parse_infix_expr(Tracker, Ptr<Expr>&& lhs, Tok::Prec = Tok::Prec::Bot);
    Ptr<Expr> parse_extract_expr(Tracker, Ptr<Expr>&&, Tok::Prec);
    ///@}

    /// @name parse primary exprs
    ///@{
    template<bool> Ptr<Expr> parse_arr_or_pack_expr();
    Ptr<Expr> parse_block_expr(std::string_view ctxt); ///< Empty @p ctxt means an explicit BlockExpr `{ d* e }`.
    Ptr<Expr> parse_lit_expr();
    Ptr<Expr> parse_extremum_expr();
    Ptr<Expr> parse_type_expr();
    Ptr<Expr> parse_ret_expr();
    Ptr<PiExpr> parse_pi_expr();
    Ptr<LamExpr> parse_lam_expr();
    Ptr<Expr> parse_sigma_expr();
    Ptr<Expr> parse_tuple_expr();
    Ptr<Expr> parse_insert_expr();
    ///@}

    /// @name parse ptrns
    ///@{

    /// Depending on @p tag, this parses a `()`-style (Tok::Tag::D_paren_l) or `[]`-style (Tok::Tag::D_brckt_l) Ptrn.
    Ptr<Ptrn> parse_ptrn(Tok::Tag tag, std::string_view ctxt, Tok::Prec = Tok::Prec::Bot, bool allow_annex = false);
    Ptr<TuplePtrn> parse_tuple_ptrn(bool rebind, Dbg);
    ///@}

    /// @name parse decls
    ///@{

    /// If @p ctxt ...
    /// * ... empty: **Only** decls are parsed. @returns `nullptr`
    /// * ... **non**-empty: Decls are parsed, then an expression. @returns expression.
    Ptrs<Decl> parse_decls();
    Ptr<Decl> parse_axiom_decl();
    Ptr<Decl> parse_let_decl();
    Ptr<PiDecl> parse_pi_decl();
    Ptr<LamDecl> parse_lam_decl();
    Ptr<SigmaDecl> parse_sigma_decl();
    ///@}

    /// @name error messages
    ///@{
    /// Issue an error message of the form:
    /// "expected \<what\>, got '\<tok>\' while parsing \<ctxt\>"
    void syntax_err(std::string_view what, const Tok& tok, std::string_view ctxt) {
        ast().error(tok.loc(), "expected {}, got '{}' while parsing {}", what, tok, ctxt);
    }

    /// Same above but uses @p ahead() as @p tok.
    void syntax_err(std::string_view what, std::string_view ctxt) { syntax_err(what, ahead(), ctxt); }

    void syntax_err(Tok::Tag tag, std::string_view ctxt) {
        std::string msg("'");
        msg.append(Tok::tag2str(tag)).append("'");
        syntax_err(msg, ctxt);
    }
    ///@}

    AST& ast_;
    Lexer* lexer_ = nullptr;

    friend class fe::Parser<Tok, Tok::Tag, Look_Ahead, Parser>;
};

} // namespace thorin::ast
