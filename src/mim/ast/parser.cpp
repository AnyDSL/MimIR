#include "mim/ast/parser.h"

#include <filesystem>
#include <fstream>

#include "mim/driver.h"

// clang-format off
#define C_PRIMARY     \
              K_Univ: \
    case Tag::K_Nat:  \
    case Tag::K_Idx:  \
    case Tag::K_Bool: \
    case Tag::K_ff:   \
    case Tag::K_tt:   \
    case Tag::K_i1:   \
    case Tag::K_i8:   \
    case Tag::K_i16:  \
    case Tag::K_i32:  \
    case Tag::K_i64:  \
    case Tag::K_I1:   \
    case Tag::K_I8:   \
    case Tag::K_I16:  \
    case Tag::K_I32:  \
    case Tag::K_I64:  \
    case Tag::T_star: \
    case Tag::T_box

#define C_ID         \
              M_anx: \
    case Tag::M_id

#define C_LIT        \
              T_bot: \
    case Tag::T_top: \
    case Tag::L_str: \
    case Tag::L_c:   \
    case Tag::L_s:   \
    case Tag::L_u:   \
    case Tag::L_f:   \
    case Tag::L_i

#define C_LAM        \
              K_lam: \
    case Tag::K_con: \
    case Tag::K_fun

#define C_DECL        \
              K_ax:   \
    case Tag::K_let:  \
    case Tag::K_rec:  \
    case Tag::K_ccon: \
    case Tag::K_cfun: \
    case Tag::C_LAM

#define C_PI             \
              T_Pi:      \
    case Tag::K_Cn:      \
    case Tag::K_Fn

#define C_LM        \
              T_lm: \
    case Tag::K_cn: \
    case Tag::K_fn

#define C_EXPR                          \
              C_PRIMARY:                \
    case Tag::C_ID:                     \
    case Tag::C_LIT:                    \
    case Tag::C_DECL:                   \
    case Tag::C_PI:                     \
    case Tag::C_LM:                     \
    case Tag::K_Type:    /*TypeExpr*/   \
    case Tag::K_ins:     /*InsertExpr*/ \
    case Tag::K_ret:     /*RetExpr*/    \
    case Tag::D_angle_l: /*PackExpr*/   \
    case Tag::D_brckt_l: /*SigmaExpr*/  \
    case Tag::D_paren_l: /*TupleExpr*/  \
    case Tag::D_quote_l  /*ArrExpr*/

#define C_PTRN            \
              M_id:       \
    case Tag::T_backtick: \
    case Tag::D_brckt_l:  \
    case Tag::D_paren_l
// clang-format on

using namespace std::string_literals;

namespace mim::ast {

using Tag = Tok::Tag;

/*
 * entry points
 */

Ptr<Module> Parser::parse_module() {
    auto track = tracker();
    Ptrs<Import> imports;
    while (true) {
        if (ahead().isa(Tag::K_import) || ahead().isa(Tag::K_plugin)) {
            if (auto import = parse_import_or_plugin()) imports.emplace_back(std::move(import));
        } else {
            break;
        }
    }
    auto decls = parse_decls();
    bool where = ahead().isa(Tag::K_where);
    expect(Tag::EoF, "module");
    auto mod = ptr<Module>(track, std::move(imports), std::move(decls));
    if (where) ast().note(mod->loc().anew_finis(), "did you accidentally end your declaration expression with a ';'?");
    return mod;
}

Ptr<Module> Parser::import(Dbg dbg, std::ostream* md) {
    auto name     = dbg.sym();
    auto filename = fs::path(name.view());
    driver().VLOG("import: {}", name);

    if (!filename.has_extension()) filename.replace_extension("mim"); // TODO error cases

    fs::path rel_path;
    for (const auto& path : driver().search_paths()) {
        std::error_code ignore;
        rel_path = path / filename;
        if (bool reg_file = fs::is_regular_file(rel_path, ignore); reg_file && !ignore) break;
        rel_path = path / name.view() / filename;
        if (bool reg_file = fs::is_regular_file(rel_path, ignore); reg_file && !ignore) break;
    }

    if (auto path = driver().add_import(std::move(rel_path), name)) {
        auto ifs = std::ifstream(*path);
        return import(ifs, dbg.loc(), path, md);
    }
    return {};
}

Ptr<Module> Parser::import(std::istream& is, Loc loc, const fs::path* path, std::ostream* md) {
    driver().VLOG("reading: {}", path ? path->string() : "<unknown file>"s);
    if (!is) {
        ast().error(loc, "cannot read file {}", *path);
        return {};
    }

    auto state = std::tuple(prev_, ahead_, lexer_);
    auto lexer = Lexer(ast(), is, path, md);
    lexer_     = &lexer;
    init(path);
    auto mod                        = parse_module();
    std::tie(prev_, ahead_, lexer_) = state;
    return mod;
}

Ptr<Module> Parser::plugin(Dbg dbg) {
    if (!driver().flags().bootstrap && !driver().is_loaded(dbg.sym())) driver().load(dbg.sym());
    return import(dbg);
}

/*
 * misc
 */

Ptr<Import> Parser::parse_import_or_plugin() {
    auto track = tracker();
    auto tag   = lex().tag();
    auto name  = expect(Tag::M_id, "{} name", tag == Tag::K_import ? "import" : "plugin");
    auto dbg   = name.dbg();
    expect(Tag::T_semicolon, "end of {}", tag == Tag::K_import ? "import" : "plugin");
    if (auto module = tag == Tag::K_import ? import(dbg) : plugin(dbg))
        return ptr<Import>(track, tag, name.dbg(), std::move(module));
    return {};
}

Dbg Parser::parse_id(std::string_view ctxt) {
    if (auto id = accept(Tag::M_id)) return id.dbg();
    syntax_err("identifier", ctxt);
    return {prev_, driver().sym("<error>")};
}

Dbg Parser::parse_name(std::string_view ctxt) {
    if (auto tok = accept(Tag::M_anx)) return tok.dbg();
    if (auto tok = accept(Tag::M_id)) return tok.dbg();
    syntax_err("identifier or annex name", ctxt);
    return Dbg(prev_, ast().sym("<error>"));
}

Ptr<Expr> Parser::parse_type_ascr(std::string_view ctxt) {
    if (accept(Tag::T_colon)) return parse_expr(ctxt);
    if (ctxt.empty()) return nullptr;
    syntax_err("':'", ctxt);
    return ptr<ErrorExpr>(prev_);
}

/*
 * exprs
 */

Ptr<Expr> Parser::parse_expr(std::string_view ctxt, Prec curr_prec) {
    auto track = tracker();
    auto lhs   = parse_primary_expr(ctxt);
    return parse_infix_expr(track, std::move(lhs), curr_prec);
}

Ptr<Expr> Parser::parse_infix_expr(Tracker track, Ptr<Expr>&& lhs, Prec curr_prec) {
    while (true) {
        // If operator in ahead has less left precedence: reduce (break).
        switch (ahead().tag()) {
            case Tag::T_extract: {
                if (curr_prec >= Prec::Extract) return lhs;
                lex();
                if (auto tok = accept(Tag::M_id))
                    lhs = ptr<ExtractExpr>(track.loc(), std::move(lhs), tok.dbg());
                else {
                    auto rhs = parse_expr("right-hand side of an extract", Prec::Extract);
                    lhs      = ptr<ExtractExpr>(track.loc(), std::move(lhs), std::move(rhs));
                }
                continue;
            }
            case Tag::T_arrow: {
                if (curr_prec > Prec::Arrow) return lhs; // ">" - Arrow is rassoc
                lex();
                auto rhs = parse_expr("right-hand side of a function type", Prec::Arrow);
                lhs      = ptr<ArrowExpr>(track.loc(), std::move(lhs), std::move(rhs));
                continue;
            }
            case Tag::T_at: {
                if (curr_prec >= Prec::App) return lhs;
                lex();
                auto rhs = parse_expr("explicit argument to an application", Prec::App);
                lhs      = ptr<AppExpr>(track.loc(), true, std::move(lhs), std::move(rhs));
                continue;
            }
            case Tag::C_EXPR: {
                if (curr_prec >= Prec::App) return lhs;
                switch (ahead().tag()) {
                    case Tag::C_DECL:
                        ast().warn(ahead().loc(), "you are passing a declaration expression as argument");
                        ast().note(lhs->loc(), "to this expression");
                        ast().note(ahead().loc(),
                                   "if this was your intention, consider to parenthesize the declaration expression");
                        ast().note(lhs->loc().anew_finis(), "otherwise, you are probably missing a ';'");
                    default: break;
                }
                auto rhs = parse_expr("argument to an application", Prec::App);
                lhs      = ptr<AppExpr>(track.loc(), false, std::move(lhs), std::move(rhs));
                continue;
            }
            case Tag::K_where: {
                if (curr_prec >= Prec::Where) return lhs;
                lex();
                auto decls = parse_decls();
                lhs        = ptr<DeclExpr>(track, std::move(decls), std::move(lhs), true);

                bool where = ahead().tag() == Tag::K_where;
                expect(Tag::K_end, "end of a .where declaration block");
                if (where)
                    ast().note(lhs->loc().anew_finis(),
                               "did you accidentally end your declaration expression with a ';'?");
                return lhs;
            }
            default: return lhs;
        }
    }
}

Ptr<Expr> Parser::parse_insert_expr() {
    eat(Tag::K_ins);
    auto track = tracker();
    expect(Tag::D_paren_l, "opening paren for insert arguments");
    auto tuple = parse_expr("the tuple to insert into");
    expect(Tag::T_comma, "comma after tuple to insert into");
    auto index = parse_expr("insert index");
    expect(Tag::T_comma, "comma after insert index");
    auto value = parse_expr("insert value");
    expect(Tag::D_paren_r, "closing paren for insert arguments");
    return ptr<InsertExpr>(track.loc(), std::move(tuple), std::move(index), std::move(value));
}

Ptr<Expr> Parser::parse_primary_expr(std::string_view ctxt) {
    // clang-format off
    switch (ahead().tag()) {
        case Tag::C_PRIMARY: return ptr<PrimaryExpr>(lex());
        case Tag::C_ID:      return ptr<IdExpr>(lex().dbg());
        case Tag::C_LIT:     return parse_lit_expr();
        case Tag::C_DECL:    return parse_decl_expr();
        case Tag::C_PI:      return parse_pi_expr();
        case Tag::C_LM:      return parse_lam_expr();
        case Tag::K_ins:     return parse_insert_expr();
        case Tag::K_ret:     return parse_ret_expr();
        case Tag::D_quote_l: return parse_arr_or_pack_expr<true>();
        case Tag::D_angle_l: return parse_arr_or_pack_expr<false>();
        case Tag::D_brckt_l: return parse_sigma_expr();
        case Tag::D_paren_l: return parse_tuple_expr();
        case Tag::K_Type:    return parse_type_expr();
        default:
            if (ctxt.empty()) return nullptr;
            syntax_err("primary expression", ctxt);
    }
    // clang-format on
    return ptr<ErrorExpr>(prev_);
}

template<bool arr> Ptr<Expr> Parser::parse_arr_or_pack_expr() {
    auto track = tracker();
    eat(arr ? Tag::D_quote_l : Tag::D_angle_l);

    Dbg dbg;
    if (ahead(0).isa(Tag::M_id) && ahead(1).isa(Tag::T_colon)) {
        dbg = eat(Tag::M_id).dbg();
        eat(Tag::T_colon);
    }

    auto shape = parse_expr(arr ? "shape of an array" : "shape of a pack");
    auto ptrn  = IdPtrn::mk_id(ast(), dbg, std::move(shape));
    expect(Tag::T_semicolon, arr ? "array" : "pack");
    auto body = parse_expr(arr ? "body of an array" : "body of a pack");
    expect(arr ? Tag::D_quote_r : Tag::D_angle_r,
           arr ? "closing delimiter of an array" : "closing delimiter of a pack");

    return ptr<ArrOrPackExpr<arr>>(track, std::move(ptrn), std::move(body));
}

Ptr<Expr> Parser::parse_decl_expr() {
    auto track = tracker();
    auto decls = parse_decls();
    auto expr  = parse_expr("final expression of a declaration expression");
    return ptr<DeclExpr>(track, std::move(decls), std::move(expr), false);
}

Ptr<Expr> Parser::parse_lit_expr() {
    auto track = tracker();
    auto tok   = lex();
    auto type  = accept(Tag::T_colon) ? parse_expr("literal", Prec::Lit) : nullptr;
    return ptr<LitExpr>(track, tok, std::move(type));
}

Ptr<Expr> Parser::parse_sigma_expr() { return ptr<SigmaExpr>(parse_tuple_ptrn()); }

Ptr<Expr> Parser::parse_tuple_expr() {
    auto track = tracker();
    Ptrs<Expr> elems;
    parse_list("tuple", Tag::D_paren_l, [&]() { elems.emplace_back(parse_expr("tuple element")); });
    return ptr<TupleExpr>(track, std::move(elems));
}

Ptr<Expr> Parser::parse_type_expr() {
    auto track = tracker();
    eat(Tag::K_Type);
    auto level = parse_expr("type level", Prec::App);
    return ptr<TypeExpr>(track, std::move(level));
}

Ptr<Expr> Parser::parse_pi_expr() {
    auto track = tracker();
    auto tag   = lex().tag();

    std::string entity;
    switch (tag) {
        case Tag::T_Pi: entity = "dependent function type"; break;
        case Tag::K_Cn: entity = "continuation type"; break;
        case Tag::K_Fn: entity = "returning continuation type"; break;
        default: fe::unreachable();
    }

    Ptrs<PiExpr::Dom> doms;
    while (true) {
        auto track    = tracker();
        auto implicit = (bool)accept(Tag::T_dot);
        auto prec     = tag == Tag::K_Cn ? Prec::Bot : Prec::Pi;
        auto ptrn     = parse_ptrn(Tag::D_brckt_l, "domain of a "s + entity, prec);
        doms.emplace_back(ptr<PiExpr::Dom>(track, implicit, std::move(ptrn)));

        switch (ahead().tag()) {
            case Tag::C_PTRN: continue;
            default: break;
        }
        break;
    }

    auto codom = tag != Tag::K_Cn ? (expect(Tag::T_arrow, entity), parse_expr("codomain of a "s + entity, Prec::Arrow))
                                  : nullptr;

    if (tag == Tag::K_Fn) doms.back()->add_ret(ast(), codom ? std::move(codom) : ptr<InferExpr>(prev_));

    return ptr<PiExpr>(track.loc(), tag, std::move(doms), std::move(codom));
}

Ptr<Expr> Parser::parse_lam_expr() { return ptr<LamExpr>(parse_lam_decl()); }

Ptr<Expr> Parser::parse_ret_expr() {
    auto track = tracker();
    eat(Tag::K_ret);
    auto ptrn = parse_ptrn(Tag::D_paren_l, "binding pattern of a ret expression");
    expect(Tag::T_assign, "ret expression");
    auto callee = parse_expr("continuation expression of a ret expression");
    expect(Tag::T_dollar, "separator of a ret expression");
    auto arg = parse_expr("argument of ret expression");
    expect(Tag::T_semicolon, "ret expression");
    auto body = parse_expr("body of a ret expression");
    return ptr<RetExpr>(track, std::move(ptrn), std::move(callee), std::move(arg), std::move(body));
}

/*
 * ptrns
 */

Ptr<Ptrn> Parser::parse_ptrn(Tag delim_l, std::string_view ctxt, Prec prec, bool allow_annex) {
    auto track = tracker();
    bool p     = delim_l == Tag::D_paren_l;
    bool b     = delim_l == Tag::D_brckt_l;
    assert((p ^ b) && "left delimiter must either be '(' or '['");

    // p -> anx
    // p -> anx: e
    if (allow_annex) {
        assert(p);
        if (auto anx = accept(Tok::Tag::M_anx)) {
            auto type = parse_type_ascr();
            return ptr<IdPtrn>(track, false, anx.dbg(), std::move(type));
        }
    }

    // p -> `s: e   b -> `s: e
    if (accept(Tok::Tag::T_backtick)) {
        auto dbg  = expect(Tok::Tag::M_id, "identifier pattern").dbg();
        auto type = accept(Tok::Tag::T_colon) ? parse_expr("identifier pattern", prec) : nullptr;
        return ptr<IdPtrn>(track, true, dbg, std::move(type));
    }

    // p -> (p, ..., p)
    // p -> [b, ..., b]     b -> [b, ..., b]
    if ((p && ahead().isa(Tag::D_paren_l)) || ahead().isa(Tag::D_brckt_l)) return parse_tuple_ptrn();

    if (ahead(0).isa(Tag::M_id)) {
        if (ahead(1).isa(Tag::T_colon)) {
            // p ->  s: e       b ->  s: e
            auto dbg = eat(Tag::M_id).dbg();
            eat(Tag::T_colon);
            auto type = parse_expr(ctxt, prec);
            return ptr<IdPtrn>(track, false, dbg, std::move(type));
        } else if (p) {
            // p ->  s
            // p -> `s
            auto dbg = eat(Tag::M_id).dbg();
            return ptr<IdPtrn>(track, false, dbg, nullptr);
        } else {
            // b -> e   where e == id
            auto type = parse_expr(ctxt, prec);
            return ptr<IdPtrn>(track, false, type->loc().anew_begin(), std::move(type));
        }
    } else if (b) {
        // b -> e   where e != id
        auto type = parse_expr(ctxt, prec);
        auto loc  = type->loc().anew_begin();
        return ptr<IdPtrn>(track, false, Dbg(loc), std::move(type));
    } else if (!ctxt.empty()) {
        // p -> ↯
        syntax_err("pattern", ctxt);
        return ptr<ErrorPtrn>(prev_);
    }

    return nullptr;
}

Ptr<TuplePtrn> Parser::parse_tuple_ptrn() {
    auto track   = tracker();
    auto delim_l = ahead().tag();

    Ptrs<Ptrn> ptrns;
    parse_list("tuple pattern", delim_l, [&]() {
        auto track = tracker();
        Ptr<Ptrn> ptrn;

        if (ahead(0).isa(Tag::M_id) && ahead(1).isa(Tag::M_id)) {
            Dbgs dbgs;
            while (auto tok = accept(Tag::M_id)) dbgs.emplace_back(tok.dbg());

            if (accept(Tag::T_colon)) { // identifier group: x y x: T
                auto dbg  = dbgs.back();
                auto type = parse_expr("type of an identifier group within a tuple pattern");
                auto id   = ptr<IdPtrn>(dbg.loc() + type->loc().finis, false, dbg, std::move(type));

                for (auto dbg : dbgs | std::views::take(dbgs.size() - 1))
                    ptrns.emplace_back(ptr<GrpPtrn>(dbg, id.get()));
                ptrns.emplace_back(std::move(id));
                return;
            }

            // "x y z" is a curried app and maybe the prefix of a longer type expression
            Ptr<Expr> lhs = ptr<IdExpr>(dbgs.front());
            for (auto dbg : dbgs | std::views::drop(1)) {
                auto rhs = ptr<IdExpr>(dbg);
                lhs      = ptr<AppExpr>(track, false, std::move(lhs), std::move(rhs));
            }
            auto expr = parse_infix_expr(track, std::move(lhs), Prec::App);
            ptrn      = IdPtrn::mk_type(ast(), std::move(expr));
        } else {
            auto dl = delim_l == Tag::D_brckt_l ? delim_l : Tag::D_paren_l;
            ptrn    = parse_ptrn(dl, "element of a tuple pattern");

            if (delim_l == Tag::D_brckt_l) {
                // If we are able to parse more stuff, we got an expr instead of a binder:
                // [..., [.Nat, .Nat] -> .Nat, ...] ==> [..., _: [.Nat, .Nat] -> .Nat, ...]
                if (auto expr = Ptrn::to_expr(ast(), std::move(ptrn))) {
                    auto addr = expr.get();
                    expr      = parse_infix_expr(track, std::move(expr));
                    if (expr.get() != addr) {
                        auto loc = expr->loc();
                        ptrn     = ptr<IdPtrn>(loc, false, Dbg(loc.anew_begin(), Sym()), std::move(expr));
                    } else {
                        if (!ptrn) ptrn = Ptrn::to_ptrn(std::move(expr));
                    }
                }
            }
        }

        ptrns.emplace_back(std::move(ptrn));
    });

    Dbg dbg;
    bool rebind = false;
    if (accept(Tag::T_colon_colon)) {
        rebind = (bool)accept(Tag::T_backtick);
        dbg    = eat(Tag::M_id).dbg();
    }
    return ptr<TuplePtrn>(track, delim_l, std::move(ptrns), rebind, dbg);
}

/*
 * decls
 */

Ptrs<ValDecl> Parser::parse_decls() {
    Ptrs<ValDecl> decls;
    while (true) {
        // clang-format off
        switch (ahead().tag()) {
            case Tag::T_semicolon: lex(); break; // eat up stray semicolons
            case Tag::K_ax:        decls.emplace_back(parse_axiom_decl());        break;
            case Tag::K_ccon:
            case Tag::K_cfun:      decls.emplace_back(parse_c_decl());            break;
            case Tag::K_let:       decls.emplace_back(parse_let_decl());          break;
            case Tag::K_rec:       decls.emplace_back(parse_rec_decl(true));      break;
            case Tag::K_con:
            case Tag::K_fun:
            case Tag::K_lam:       decls.emplace_back(parse_lam_decl());          break;
            default:               return decls;
        }
        // clang-format on
    }
}

Ptr<ValDecl> Parser::parse_axiom_decl() {
    auto track = tracker();
    eat(Tag::K_ax);
    Dbg dbg, normalizer;
    Tok curry, trip;
    if (auto name = expect(Tag::M_anx, "annex name of an axiom"))
        dbg = name.dbg();
    else
        dbg = Dbg(prev_, ast().sym("<error annex name>"));

    std::deque<Ptrs<AxiomDecl::Alias>> subs;
    if (ahead().isa(Tag::D_paren_l)) {
        parse_list("tag list of an axiom", Tag::D_paren_l, [&]() {
            auto& aliases = subs.emplace_back();
            aliases.emplace_back(ptr<AxiomDecl::Alias>(parse_id("tag of an axiom")));
            while (accept(Tag::T_assign))
                aliases.emplace_back(ptr<AxiomDecl::Alias>(parse_id("alias of an axiom tag")));
        });
    }

    auto type = parse_type_ascr("type ascription of an axiom");

    if (ahead(0).isa(Tag::T_comma) && ahead(1).isa(Tag::M_id)) {
        lex();
        normalizer = lex().dbg();
    }
    if (accept(Tag::T_comma)) {
        if (auto c = expect(Tag::L_u, "curry counter for axiom")) curry = c;
        if (accept(Tag::T_comma)) {
            if (auto t = expect(Tag::L_u, "trip count for axiom")) trip = t;
        }
    }

    return ptr<AxiomDecl>(track, dbg, std::move(subs), std::move(type), normalizer, curry, trip);
}

Ptr<ValDecl> Parser::parse_let_decl() {
    auto track = tracker();
    eat(Tag::K_let);
    auto ptrn = parse_ptrn(Tag::D_paren_l, "binding pattern of a let declaration", Prec::Bot, true);
    expect(Tag::T_assign, "let");
    auto type  = parse_type_ascr();
    auto value = parse_expr("value of a let declaration");
    return ptr<LetDecl>(track, std::move(ptrn), std::move(value));
}

Ptr<ValDecl> Parser::parse_c_decl() {
    auto track = tracker();
    auto tag   = lex().tag();
    auto id    = expect(Tag::M_id, "C function declaration");
    auto dom   = parse_ptrn(Tag::D_brckt_l, "domain of a C function"s, Prec::App);
    Ptr<Expr> codom;
    if (tag == Tag::K_cfun) {
        expect(Tag::T_colon, "codomain of a C function");
        codom = parse_expr("codomain of a C function");
    }
    return ptr<CDecl>(track, tag, id.dbg(), std::move(dom), std::move(codom));
}

Ptr<RecDecl> Parser::parse_rec_decl(bool first) {
    auto track = tracker();
    eat(first ? Tag::K_rec : Tag::K_and);
    auto dbg  = parse_name("recursive declaration");
    auto type = accept(Tag::T_colon) ? parse_expr("type of a recursive declaration") : ptr<InferExpr>(prev_);
    expect(Tag::T_assign, "recursive declaration");
    auto body = parse_expr("body of a recursive declaration");
    auto next = ahead().isa(Tag::K_and) ? parse_and_decl() : nullptr;
    return ptr<RecDecl>(track, dbg, std::move(type), std::move(body), std::move(next));
}

Ptr<LamDecl> Parser::parse_lam_decl() {
    auto track    = tracker();
    auto tag      = lex().tag();
    auto prec     = tag == Tag::K_cn || tag == Tag::K_con ? Prec::Bot : Prec::Pi;
    bool external = (bool)accept(Tag::K_extern);

    bool decl;
    std::string entity;
    // clang-format off
    switch (tag) {
        case Tag::T_lm:  decl = false; entity = "function expression";                break;
        case Tag::K_cn:  decl = false; entity = "continuation expression";            break;
        case Tag::K_fn:  decl = false; entity = "returning continuation expression";  break;
        case Tag::K_lam: decl = true ; entity = "function declaration";               break;
        case Tag::K_con: decl = true ; entity = "continuation declaration";           break;
        case Tag::K_fun: decl = true ; entity = "returning continuation declaration"; break;
        default: fe::unreachable();
    }
    // clang-format on

    auto dbg = decl ? parse_name(entity) : Dbg();
    Ptrs<LamDecl::Dom> doms;
    while (true) {
        auto track    = tracker();
        bool implicit = (bool)accept(Tag::T_dot);
        auto ptrn     = parse_ptrn(Tag::D_paren_l, "domain pattern of a "s + entity, prec);
        auto filter   = accept(Tag::T_at) ? parse_expr("filter") : nullptr;

        doms.emplace_back(ptr<LamDecl::Dom>(track, implicit, std::move(ptrn), std::move(filter)));
        switch (ahead().tag()) {
            case Tag::C_PTRN: continue;
            default: break;
        }
        break;
    }

    auto codom = accept(Tag::T_colon) ? parse_expr("codomain of a "s + entity, Prec::Arrow) : nullptr;
    if (tag == Tag::K_fn || tag == Tag::K_fun)
        doms.back()->add_ret(ast(), codom ? std::move(codom) : ptr<InferExpr>(prev_));

    expect(Tag::T_assign, "body of a "s + entity);
    auto body = parse_expr("body of a "s + entity);
    auto next = ahead().isa(Tag::K_and) ? parse_and_decl() : nullptr;

    return ptr<LamDecl>(track, tag, external, dbg, std::move(doms), std::move(codom), std::move(body), std::move(next));
}

Ptr<RecDecl> Parser::parse_and_decl() {
    switch (ahead(1).tag()) {
        case Tag::C_LAM: return lex(), parse_lam_decl();
        default: return parse_rec_decl(false);
    }
}

} // namespace mim::ast
