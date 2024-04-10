#include "thorin/ast/parser.h"

#include <filesystem>
#include <fstream>

#include "thorin/driver.h"

using namespace std::string_literals;

namespace thorin::ast {

using Tag = Tok::Tag;

/*
 * entry points
 */

Ptr<Module> Parser::parse_module() {
    auto track = tracker();
    std::deque<Import> imports;
    while (true)
        if (ahead().tag() == Tag::K_import)
            imports.emplace_back(parse_import());
        else if (ahead().tag() == Tag::K_plugin)
            imports.emplace_back(parse_plugin());
        else
            break;

    auto decls = parse_decls();
    expect(Tag::EoF, "module");
    return ptr<Module>(track, std::move(imports), std::move(decls));
}

Ptr<Module> Parser::import(Sym name, std::ostream* md) {
    driver().VLOG("import: {}", name);
    auto filename = fs::path(name.view());

    if (!filename.has_extension()) filename.replace_extension("thorin"); // TODO error cases

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
        return import(ifs, path, md);
    }
    return {};
}

Ptr<Module> Parser::import(std::istream& is, const fs::path* path, std::ostream* md) {
    driver().VLOG("reading: {}", path ? path->string() : "<unknown file>"s);
    if (!is) return {};

    auto state = std::tuple(prev_, ahead_, lexer_);
    auto lexer = Lexer(ast(), is, path, md);
    lexer_     = &lexer;
    init(path);
    auto mod                        = parse_module();
    std::tie(prev_, ahead_, lexer_) = state;
    return mod;
}

Ptr<Module> Parser::plugin(Sym name) {
    if (!driver().flags().bootstrap && !driver().is_loaded(name)) driver().load(name);
    return import(name);
}

/*
 * misc
 */

Import Parser::parse_import() {
    eat(Tag::K_import);
    auto name = expect(Tag::M_id, "import name");
    expect(Tag::T_semicolon, "end of import");
    auto module = import(name.sym());
    return Import(name.dbg(), Tag::K_import, std::move(module));
}

Import Parser::parse_plugin() {
    eat(Tag::K_plugin);
    auto name = expect(Tag::M_id, "thorin/plugin name");
    expect(Tag::T_semicolon, "end of import");
    auto module = plugin(name.sym());
    return Import(name.dbg(), Tag::K_plugin, std::move(module));
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
    if (accept(Tag::T_colon)) return parse_expr(ctxt, Tok::Prec::Bot);
    if (ctxt.empty()) return nullptr;
    syntax_err("':'", ctxt);
    return ptr<ErrorExpr>(prev_);
}

/*
 * exprs
 */

Ptr<Expr> Parser::parse_expr(std::string_view ctxt, Tok::Prec p) {
    auto track = tracker();
    auto lhs   = parse_primary_expr(ctxt);
    return parse_infix_expr(track, std::move(lhs), p);
}

Ptr<Expr> Parser::parse_infix_expr(Tracker track, Ptr<Expr>&& lhs, Tok::Prec p) {
    while (true) {
        // If operator in ahead has less left precedence: reduce (break).
        if (ahead().isa(Tag::T_extract)) {
            if (auto extract = parse_extract_expr(track, std::move(lhs), p))
                lhs = std::move(extract);
            else
                break;
        } else if (ahead().isa(Tag::T_arrow)) {
            auto [l, r] = Tok::prec(Tok::Prec::Arrow);
            if (l < p) break;
            lex();
            auto rhs = parse_expr("right-hand side of an function type", r);
            lhs      = ptr<ArrowExpr>(track.loc(), std::move(lhs), std::move(rhs));
        } else {
            auto [l, r] = Tok::prec(Tok::Prec::App);
            if (l < p) break;
            bool is_explicit = (bool)accept(Tag::T_at);
            if (auto rhs = parse_expr({}, r)) // if we can parse an expression, it's an App
                lhs = ptr<AppExpr>(track.loc(), is_explicit, std::move(lhs), std::move(rhs));
            else
                return lhs;
        }
    }

    return lhs;
}

Ptr<Expr> Parser::parse_extract_expr(Tracker track, Ptr<Expr>&& lhs, Tok::Prec p) {
    auto [l, r] = Tok::prec(Tok::Prec::Extract);
    if (l < p) return nullptr;
    lex();
    if (auto tok = accept(Tag::M_id)) return ptr<ExtractExpr>(track.loc(), std::move(lhs), tok.dbg());
    auto rhs = parse_expr("right-hand side of an extract", r);
    return ptr<ExtractExpr>(track.loc(), std::move(lhs), std::move(rhs));
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
        case Tag::K_Cn:
        case Tag::K_Fn:
        case Tag::T_Pi:      return parse_pi_expr();
        case Tag::K_cn:
        case Tag::K_fn:
        case Tag::T_lm:      return parse_lam_expr();
        case Tag::K_ins:     return parse_insert_expr();
        case Tag::K_ret:     return parse_ret_expr();
        case Tag::D_quote_l: return parse_arr_or_pack_expr<true>();
        case Tag::D_angle_l: return parse_arr_or_pack_expr<false>();
        case Tag::D_brckt_l: return parse_sigma_expr();
        case Tag::D_paren_l: return parse_tuple_expr();
        case Tag::D_brace_l: return parse_block_expr({});
        case Tag::K_Type:    return parse_type_expr();

        case Tag::K_Univ:
        case Tag::K_Nat:
        case Tag::K_Idx:
        case Tag::K_Bool:
        case Tag::K_ff:
        case Tag::K_tt:
        case Tag::K_i1:
        case Tag::K_i8:
        case Tag::K_i16:
        case Tag::K_i32:
        case Tag::K_i64:
        case Tag::K_I1:
        case Tag::K_I8:
        case Tag::K_I16:
        case Tag::K_I32:
        case Tag::K_I64:
        case Tag::M_char:
        case Tag::T_star:
        case Tag::T_box: {
            auto tok = lex();
            return ptr<PrimaryExpr>(tok);
        }
        case Tag::M_lit: return parse_lit_expr();
        case Tag::T_bot:
        case Tag::T_top: return parse_extremum_expr();
        case Tag::M_anx:
        case Tag::M_id:      return ptr<IdExpr>(lex().dbg());
        //case Tag::M_str:     return world().tuple(lex().sym())->set(prev_);
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
    expect(Tag::T_semicolon, arr ? "array" : "pack");
    auto body = parse_expr(arr ? "body of an array" : "body of a pack");
    expect(arr ? Tag::D_quote_r : Tag::D_angle_r,
           arr ? "closing delimiter of an array" : "closing delimiter of a pack");

    return ptr<ArrOrPackExpr<arr>>(track, dbg, std::move(shape), std::move(body));
}

Ptr<Expr> Parser::parse_block_expr(std::string_view ctxt) {
    auto track = tracker();
    if (ctxt.empty()) eat(Tag::D_brace_l);
    auto decls = parse_decls();
    auto expr  = parse_expr("final expression in a "s + (ctxt.empty() ? "block expressoin"s : std::string(ctxt)));
    if (ctxt.empty()) expect(Tag::D_brace_r, "block expression");
    return ptr<BlockExpr>(track, /*has_braces*/ ctxt.empty(), std::move(decls), std::move(expr));
}

Ptr<Expr> Parser::parse_extremum_expr() {
    auto track  = tracker();
    auto tag    = lex().tag();
    auto [_, r] = Tok::prec(Tok::Prec::Lit);
    auto type   = accept(Tag::T_colon) ? parse_expr("type of "s + Tok::tag2str(tag), r) : nullptr;
    return ptr<ExtremumExpr>(track, tag, std::move(type));
}

Ptr<Expr> Parser::parse_lit_expr() {
    auto track  = tracker();
    auto value  = lex();
    auto [_, r] = Tok::prec(Tok::Prec::Lit);
    auto type   = accept(Tag::T_colon) ? parse_expr("literal", r) : nullptr;
    return ptr<LitExpr>(track, value.dbg(), std::move(type));
}

Ptr<Expr> Parser::parse_sigma_expr() { return ptr<SigmaExpr>(parse_tuple_ptrn(false, Dbg(ahead().loc(), Sym()))); }

Ptr<Expr> Parser::parse_tuple_expr() {
    auto track = tracker();
    Ptrs<Expr> elems;
    parse_list("tuple", Tag::D_paren_l, [&]() { elems.emplace_back(parse_expr("tuple element")); });
    return ptr<TupleExpr>(track, std::move(elems));
}

Ptr<Expr> Parser::parse_type_expr() {
    auto track = tracker();
    eat(Tag::K_Type);
    auto [l, r] = Tok::prec(Tok::Prec::App);
    auto level  = parse_expr("type level", r);
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
    do {
        auto track    = tracker();
        auto implicit = (bool)accept(Tag::T_dot);
        auto prec     = tag == Tag::K_Cn ? Tok::Prec::Bot : Tok::Prec::App;
        auto ptrn     = parse_ptrn(Tag::D_brckt_l, "domain of a "s + entity, prec);
        doms.emplace_back(ptr<PiExpr::Dom>(track, implicit, std::move(ptrn)));
    } while (ahead().isa(Tag::T_dot) || ahead().isa(Tag::D_brckt_l) || ahead().isa(Tag::T_backtick)
             || (ahead(0).isa(Tag::M_id) && ahead(1).isa(Tag::T_colon_colon)));

    auto codom = tag != Tag::K_Cn
                   ? (expect(Tag::T_arrow, entity), parse_expr("codomain of a "s + entity, Tok::Prec::Arrow))
                   : ptr<InferExpr>(prev_);

    return ptr<PiExpr>(track.loc(), tag, std::move(doms), std::move(codom));
}

Ptr<Expr> Parser::parse_lam_expr() { return ptr<LamExpr>(parse_lam_decl()); }

Ptr<Expr> Parser::parse_ret_expr() {
    auto track = tracker();
    eat(Tag::K_ret);
    auto ptrn = parse_ptrn(Tag::D_paren_l, "binding pattern of a ret expression");
    expect(Tag::T_assign, "let expression");
    auto callee = parse_expr("continuation expression of a ret expression");
    expect(Tag::T_dollar, "separator of a ret expression");
    auto arg = parse_expr("argument of ret expression");
    expect(Tag::T_semicolon, "let expression");
    auto decls = parse_block_expr("body of a ret expression");
    return ptr<RetExpr>(track, std::move(ptrn), std::move(callee), std::move(arg), std::move(decls));
}

/*
 * ptrns
 */

Ptr<Ptrn> Parser::parse_ptrn(Tag delim_l, std::string_view ctxt, Tok::Prec prec, bool allow_annex) {
    auto track = tracker();
    auto dbg   = Dbg(ahead().loc(), Sym());
    bool p     = delim_l == Tag::D_paren_l;
    bool b     = delim_l == Tag::D_brckt_l;
    assert((p ^ b) && "left delimiter must either be '(' or '['");

    if (allow_annex) {
        assert(p);
        // p -> anx
        // p -> anx: e
        if (auto anx = accept(Tok::Tag::M_anx)) {
            auto type = parse_type_ascr();
            return ptr<IdPtrn>(track, false, anx.dbg(), std::move(type));
        }
    }

    // p ->    (p, ..., p)
    // p ->    [b, ..., b]      b ->    [b, ..., b]
    // p ->  s::(p, ..., p)
    // p ->  s::[b, ..., b]     b ->  s::[b, ..., b]
    // p ->  s: e               b ->  s: e
    // p ->  s                  b ->    e
    // p -> 's::(p, ..., p)
    // p -> 's::[b, ..., b]     b -> 's::[b, ..., b]
    // p -> 's: e               b -> 's: e
    // p -> 's

    if (p && ahead().isa(Tag::D_paren_l)) {
        // p ->    (p, ..., p)
        return parse_tuple_ptrn(false, dbg);
    } else if (ahead().isa(Tag::D_brckt_l)) {
        // p ->    [b, ..., b]      b ->    [b, ..., b]
        return parse_tuple_ptrn(false, dbg);
    }

    auto backtick = accept(Tag::T_backtick);
    bool rebind   = (bool)backtick;

    if (ahead(0).isa(Tag::M_id)) {
        // p ->  s::(p, ..., p)
        // p ->  s::[b, ..., b]     b ->  s::[b, ..., b]
        // p ->  s: e               b ->  s: e
        // p ->  s                  b ->     e    where e == id
        // p -> 's::(p, ..., p)
        // p -> 's::[b, ..., b]     b -> 's::[b, ..., b]
        // p -> 's: e               b -> 's: e
        // p -> 's
        if (ahead(1).isa(Tag::T_colon_colon)) {
            dbg = eat(Tag::M_id).dbg();
            eat(Tag::T_colon_colon);
            if (b && ahead().isa(Tag::D_paren_l))
                ast().error(ahead().loc(), "switching from []-style patterns to ()-style patterns is not allowed");
            // b ->  s::(p, ..., p)
            // b ->  s::[b, ..., b]     b ->  s::[b, ..., b]
            // b -> 's::(p, ..., p)
            // b -> 's::[b, ..., b]     b -> 's::[b, ..., b]
            if (ahead().isa(Tag::D_paren_l) || ahead().isa(Tag::D_brckt_l))
                return parse_tuple_ptrn(rebind, dbg);
            else
                syntax_err("tuple pattern after '" + dbg.sym.str() + "::'", ctxt);
        } else if (ahead(1).isa(Tag::T_colon)) {
            // p ->  s: e               b ->  s: e
            // p -> 's: e               b -> 's: e
            dbg = eat(Tag::M_id).dbg();
            eat(Tag::T_colon);
            auto type = parse_expr(ctxt, prec);
            return ptr<IdPtrn>(track, rebind, dbg, std::move(type));
        } else {
            // p ->  s                  b ->    e    where e == id
            // p -> 's
            if (p) {
                // p ->  s
                // p -> 's
                dbg = eat(Tag::M_id).dbg();
                return ptr<IdPtrn>(track, rebind, dbg, nullptr);
            } else {
                // b ->    e    where e == id
                auto type = parse_expr(ctxt, prec);
                return ptr<IdPtrn>(track, rebind, dbg, std::move(type));
            }
        }
    } else if (b) {
        // b ->  e    where e != id
        if (backtick) ast().error(backtick.loc(), "you can only prefix identifiers with backtick for rebinding");
        auto type = parse_expr(ctxt, prec);
        return ptr<IdPtrn>(track, rebind, dbg, std::move(type));
    } else if (!ctxt.empty()) {
        // p -> ↯
        syntax_err("pattern", ctxt);
    }

    return nullptr;
}

Ptr<TuplePtrn> Parser::parse_tuple_ptrn(bool rebind, Dbg dbg) {
    auto track   = tracker();
    auto delim_l = ahead().tag();
    bool p       = delim_l == Tag::D_paren_l;
    bool b       = delim_l == Tag::D_brckt_l;
    assert(p ^ b);

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
                auto id   = ptr<IdPtrn>(dbg.loc + type->loc().finis, false, dbg, std::move(type));

                for (auto dbg : dbgs | std::views::take(dbgs.size() - 1))
                    ptrns.emplace_back(ptr<GroupPtrn>(dbg, id.get()));
                ptrns.emplace_back(std::move(id));
                return;
            }

            // "x y z" is a curried app and maybe the prefix of a longer type expression
            Ptr<Expr> lhs = ptr<IdExpr>(dbgs.front());
            for (auto dbg : dbgs | std::views::drop(1)) {
                auto rhs = ptr<IdExpr>(dbg);
                lhs      = ptr<AppExpr>(track, false, std::move(lhs), std::move(rhs));
            }
            auto [_, r] = Tok::prec(Tok::Prec::App);
            auto expr   = parse_infix_expr(track, std::move(lhs), r);
            ptrn        = IdPtrn::mk_type(ast(), std::move(expr));
        } else {
            ptrn = parse_ptrn(delim_l, "element of a tuple pattern");

            if (b) {
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

    // TODO parse type
    return ptr<TuplePtrn>(track, rebind, dbg, delim_l, std::move(ptrns));
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
            case Tag::K_ax:        decls.emplace_back(parse_axiom_decl()); break;
            case Tag::K_let:       decls.emplace_back(parse_let_decl());   break;
            case Tag::K_rec:       decls.emplace_back(parse_rec_decl());   break;
            case Tag::K_con:
            case Tag::K_fun:
            case Tag::K_lam:       decls.emplace_back(parse_lam_decl());   break;
            default:               return decls;
        }
        // clang-format on
    }
}

Ptr<ValDecl> Parser::parse_axiom_decl() {
    auto track = tracker();
    eat(Tag::K_ax);
    Dbg dbg, normalizer, curry, trip;
    if (auto name = expect(Tag::M_anx, "annex name of an axiom"))
        dbg = name.dbg();
    else
        dbg = Dbg(prev_, ast().sym("<error annex name>"));

    std::deque<Dbgs> subs;
    if (ahead().isa(Tag::D_paren_l)) {
        parse_list("tag list of an axiom", Tag::D_paren_l, [&]() {
            auto& aliases = subs.emplace_back();
            aliases.emplace_back(parse_id("tag of an axiom"));
            while (accept(Tag::T_assign)) aliases.emplace_back(parse_id("alias of an axiom tag"));
        });
    }

    auto type = parse_type_ascr("type ascription of an axiom");

    if (ahead(0).isa(Tag::T_comma) && ahead(1).isa(Tag::M_id)) {
        lex();
        normalizer = lex().dbg();
    }
    if (accept(Tag::T_comma)) {
        if (auto c = expect(Tag::M_lit, "curry counter for axiom")) curry = c.dbg();
        if (accept(Tag::T_comma)) {
            if (auto t = expect(Tag::M_lit, "trip count for axiom")) trip = t.dbg();
        }
    }

    expect(Tag::T_semicolon, "end of an axiom");
    return ptr<AxiomDecl>(track, dbg, std::move(subs), std::move(type), normalizer, curry, trip);
}

Ptr<ValDecl> Parser::parse_let_decl() {
    auto track = tracker();
    eat(Tag::K_let);
    auto ptrn = parse_ptrn(Tag::D_paren_l, "binding pattern of a let declaration", Tok::Prec::Bot, true);
    expect(Tag::T_assign, "let");
    auto type  = parse_type_ascr();
    auto value = parse_expr("value of a let declaration");
    expect(Tag::T_semicolon, "let declaration");
    return ptr<LetDecl>(track, std::move(ptrn), std::move(value));
}

Ptr<RecDecl> Parser::parse_rec_decl() {
    auto track = tracker();
    eat(Tag::K_rec);
    auto dbg  = parse_name("recursive declaration");
    auto type = accept(Tag::T_colon) ? parse_expr("type of a recursive declaration") : ptr<InferExpr>(prev_);
    expect(Tag::T_assign, "recursive declaration");
    auto body = parse_expr("body of a recursive declaration");
    expect(Tag::T_semicolon, "end of a recursive declaration");
    return ptr<RecDecl>(track, dbg, std::move(type), std::move(body));
}

Ptr<LamDecl> Parser::parse_lam_decl() {
    auto track    = tracker();
    auto tag      = lex().tag();
    auto prec     = tag == Tag::K_cn || tag == Tag::K_con ? Tok::Prec::Bot : Tok::Prec::Pi;
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
    do {
        auto track    = tracker();
        auto bang     = accept(Tag::T_bang);
        bool implicit = (bool)accept(Tag::T_dot);
        auto ptrn     = parse_ptrn(Tag::D_paren_l, "domain pattern of a "s + entity, prec);

        Ptr<Expr> filter;
        if (auto tok = accept(Tag::T_at)) {
            expect(Tag::D_paren_l, "opening parenthesis of a filter");
            filter = parse_expr("filter");
            expect(Tag::D_paren_r, "closing parenthesis of a filter");
        }

        doms.emplace_back(ptr<LamDecl::Dom>(track, bang, implicit, std::move(ptrn), std::move(filter)));
    } while (!ahead().isa(Tag::T_colon) && !ahead().isa(Tag::T_assign) && !ahead().isa(Tag::T_semicolon));

    auto codom
        = accept(Tag::T_colon) ? parse_expr("codomain of a "s + entity, Tok::Prec::Arrow) : ptr<InferExpr>(prev_);
    auto body = accept(Tag::T_assign) ? parse_block_expr("body of a "s + entity) : nullptr;

    return ptr<LamDecl>(track, tag, external, dbg, std::move(doms), std::move(codom), std::move(body));
}

} // namespace thorin::ast
