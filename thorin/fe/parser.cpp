#include "thorin/fe/parser.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

#include "thorin/check.h"
#include "thorin/def.h"
#include "thorin/driver.h"
#include "thorin/rewrite.h"

#include "thorin/util/array.h"
#include "thorin/util/sys.h"

// clang-format off
#define DECL                \
         Tok::Tag::K_ax:    \
    case Tok::Tag::K_Arr:   \
    case Tok::Tag::K_Pi:    \
    case Tok::Tag::K_Sigma: \
    case Tok::Tag::K_con:   \
    case Tok::Tag::K_def:   \
    case Tok::Tag::K_fun:   \
    case Tok::Tag::K_lam:   \
    case Tok::Tag::K_let:   \
    case Tok::Tag::K_pack:
// clang-format on

using namespace std::string_literals;

namespace thorin::fe {

using Tag = Tok::Tag;

/*
 * helpers
 */

Tok Parser::lex() {
    auto result = ahead();
    prev()      = result.loc();
    for (size_t i = 0; i < Max_Ahead - 1; ++i) ahead(i) = ahead(i + 1);
    ahead(Max_Ahead - 1) = lexer().lex();
    return result;
}

std::optional<Tok> Parser::accept(Tag tag) {
    if (tag != ahead().tag()) return {};
    return lex();
}

Tok Parser::expect(Tag tag, std::string_view ctxt) {
    if (ahead().tag() == tag) return lex();

    std::string msg("'");
    msg.append(Tok::tag2str(tag)).append("'");
    syntax_err(msg, ctxt);
    return {};
}

void Parser::syntax_err(std::string_view what, const Tok& tok, std::string_view ctxt) {
    error(tok.loc(), "expected {}, got '{}' while parsing {}", what, tok, ctxt);
}

/*
 * entry points
 */

void Parser::parse_module() {
    while (true)
        if (ahead().tag() == Tag::K_import)
            parse_import();
        else if (ahead().tag() == Tag::K_plugin)
            parse_plugin();
        else
            break;

    parse_decls({});
    expect(Tag::M_eof, "module");
};

void Parser::import(fs::path name, std::ostream* md) {
    world().VLOG("import: {}", name);
    auto filename = name;

    if (!filename.has_extension()) filename.replace_extension("thorin"); // TODO error cases

    fs::path rel_path;
    for (const auto& path : driver().search_paths()) {
        rel_path = path / filename;
        std::error_code ignore;
        if (bool reg_file = fs::is_regular_file(rel_path, ignore); reg_file && !ignore) break;
    }

    if (auto path = driver().add_import(std::move(rel_path), world().sym(name.string()))) {
        auto ifs = std::ifstream(*path);
        import(ifs, path, md);
    }
}

void Parser::import(std::istream& is, const fs::path* path, std::ostream* md) {
    world().VLOG("reading: {}", path ? path->string() : "<unknown file>"s);
    if (!is) error("cannot read file '{}'", *path);

    lexers_.emplace(world(), is, path, md);
    auto state = state_;

    for (size_t i = 0; i != Max_Ahead; ++i) ahead(i) = lexer().lex();
    prev() = Loc(path, {1, 1});

    parse_module();
    state_ = state;
    lexers_.pop();
}

void Parser::plugin(fs::path path) {
    auto sym = driver().sym(path.string());
    if (!driver().flags().bootstrap && !driver().is_loaded(sym)) driver().load(sym);
    import(path);
}

/*
 * misc
 */

void Parser::parse_import() {
    eat(Tag::K_import);
    auto name = expect(Tag::M_id, "import name");
    expect(Tag::T_semicolon, "end of import");
    import(*name.sym());
}

void Parser::parse_plugin() {
    eat(Tag::K_plugin);
    auto name = expect(Tag::M_id, "plugin name");
    expect(Tag::T_semicolon, "end of import");
    plugin(*name.sym());
}

Dbg Parser::parse_sym(std::string_view ctxt) {
    if (auto id = accept(Tag::M_id)) return {id->dbg()};
    syntax_err("identifier", ctxt);
    return {prev(), world().sym("<error>")};
}

Ref Parser::parse_type_ascr(std::string_view ctxt) {
    if (accept(Tag::T_colon)) return parse_expr(ctxt, Tok::Prec::Bot);
    if (ctxt.empty()) return nullptr;
    syntax_err("':'", ctxt);
}

/*
 * exprs
 */

Ref Parser::parse_expr(std::string_view ctxt, Tok::Prec p) {
    auto track = tracker();
    auto lhs   = parse_primary_expr(ctxt);
    return parse_infix_expr(track, lhs, p);
}

Ref Parser::parse_infix_expr(Tracker track, const Def* lhs, Tok::Prec p) {
    while (true) {
        // If operator in ahead has less left precedence: reduce (break).
        if (ahead().isa(Tag::T_extract)) {
            if (auto extract = parse_extract(track, lhs, p))
                lhs = extract;
            else
                break;
        } else if (ahead().isa(Tag::T_arrow)) {
            auto [l, r] = Tok::prec(Tok::Prec::Arrow);
            if (l < p) break;
            lex();
            auto rhs = parse_expr("right-hand side of an function type", r);
            lhs      = world().pi(lhs, rhs)->set(track.loc());
        } else {
            auto [l, r] = Tok::prec(Tok::Prec::App);
            if (l < p) break;
            if (auto rhs = parse_expr({}, r)) // if we can parse an expression, it's an App
                lhs = world().iapp(lhs, rhs)->set(track.loc());
            else
                return lhs;
        }
    }

    return lhs;
}

Ref Parser::parse_extract(Tracker track, const Def* lhs, Tok::Prec p) {
    auto [l, r] = Tok::prec(Tok::Prec::Extract);
    if (l < p) return nullptr;
    lex();

    if (ahead().isa(Tag::M_id)) {
        if (auto sigma = lhs->type()->isa_mut<Sigma>()) {
            auto tok = eat(Tag::M_id);
            if (tok.sym() == '_') error(tok.loc(), "you cannot use special symbol '_' as field access");

            if (auto i = def2fields_.find(sigma); i != def2fields_.end()) {
                if (auto& fields = i->second; fields.size() == sigma->num_ops()) {
                    for (size_t i = 0, n = sigma->num_ops(); i != n; ++i)
                        if (fields[i] == tok.sym()) return world().extract(lhs, n, i)->set(track.loc());
                }
            }
            error(tok.loc(), "could not find elemement '{}' to extract from '{}' of type '{}'", tok.sym(), lhs, sigma);
        }
    }

    auto rhs = parse_expr("right-hand side of an extract", r);
    return world().extract(lhs, rhs)->set(track.loc());
}

Ref Parser::parse_insert() {
    eat(Tag::K_ins);
    auto track = tracker();

    expect(Tag::D_paren_l, "opening paren for insert arguments");
    auto tuple = parse_expr("the tuple to insert into");
    expect(Tag::T_comma, "comma after tuple to insert into");
    auto index = parse_expr("insert index");
    expect(Tag::T_comma, "comma after insert index");
    auto value = parse_expr("insert value");
    expect(Tag::D_paren_r, "closing paren for insert arguments");

    return world().insert(tuple, index, value)->set(track.loc());
}

Ref Parser::parse_primary_expr(std::string_view ctxt) {
    // clang-format off
    switch (ahead().tag()) {
        case Tag::D_quote_l: return parse_arr();
        case Tag::D_angle_l: return parse_pack();
        case Tag::D_brace_l: return parse_block();
        case Tag::D_brckt_l: return parse_sigma();
        case Tag::D_paren_l: return parse_tuple();
        case Tag::K_Type:    return parse_type();
        case Tag::K_Univ:    lex(); return world().univ();
        case Tag::K_Bool:    lex(); return world().type_bool();
        case Tag::K_Idx:     lex(); return world().type_idx();
        case Tag::K_Nat:     lex(); return world().type_nat();
        case Tag::K_ff:      lex(); return world().lit_ff();
        case Tag::K_tt:      lex(); return world().lit_tt();
        case Tag::T_at:      return parse_var();
        case Tag::K_Cn:
        case Tag::K_Fn:
        case Tag::T_Pi:      return parse_pi();
        case Tag::K_cn:
        case Tag::K_fn:
        case Tag::K_lm:      return parse_lam();
        case Tag::T_star:    lex(); return world().type();
        case Tag::T_box:     lex(); return world().type<1>();
        case Tag::T_bot:
        case Tag::T_top:
        case Tag::L_s:
        case Tag::L_u:
        case Tag::L_r:       return parse_lit();
        case Tag::M_id:      return scopes_.find(parse_sym());
        case Tag::M_i:       return lex().index();
        case Tag::K_ins:     return parse_insert();
        case Tag::M_ax:      return scopes_.find(lex().dbg());
        default:
            if (ctxt.empty()) return nullptr;
            syntax_err("primary expression", ctxt);
    }
    // clang-format on
    return nullptr;
}

Ref Parser::parse_var() {
    eat(Tag::T_at);
    auto dbg = parse_sym("variable");
    auto mut = scopes_.find(dbg)->isa_mut();
    if (!mut) error(prev(), "variable must reference a mutable");
    return mut->var()->set(dbg);
}

Ref Parser::parse_arr() {
    auto track = tracker();
    scopes_.push();
    eat(Tag::D_quote_l);

    const Def* shape = nullptr;
    Arr* arr         = nullptr;
    if (ahead(0).isa(Tag::M_id) && ahead(1).isa(Tag::T_colon)) {
        auto id = eat(Tag::M_id);
        eat(Tag::T_colon);

        auto shape = parse_expr("shape of an array");
        auto type  = world().mut_infer_univ();
        arr        = world().mut_arr(type)->set_shape(shape);
        scopes_.bind(id.dbg(), arr->var()->set(id.dbg()));
    } else {
        shape = parse_expr("shape of an array");
    }

    expect(Tag::T_semicolon, "array");
    auto body = parse_expr("body of an array");
    expect(Tag::D_quote_r, "closing delimiter of an array");
    scopes_.pop();

    if (arr) return arr->set_body(body);
    return world().arr(shape, body)->set(track.loc());
}

Ref Parser::parse_pack() {
    // TODO This doesn't work. Rework this!
    auto track = tracker();
    scopes_.push();
    eat(Tag::D_angle_l);

    const Def* shape;
    // bool mut = false;
    if (ahead(0).isa(Tag::M_id) && ahead(1).isa(Tag::T_colon)) {
        auto id = eat(Tag::M_id);
        eat(Tag::T_colon);

        shape      = parse_expr("shape of a pack");
        auto infer = world().mut_infer(world().type_idx(shape))->set(id.sym());
        scopes_.bind(id.dbg(), infer);
    } else {
        shape = parse_expr("shape of a pack");
    }

    expect(Tag::T_semicolon, "pack");
    auto body = parse_expr("body of a pack");
    expect(Tag::D_angle_r, "closing delimiter of a pack");
    scopes_.pop();
    return world().pack(shape, body)->set(track.loc());
}

Ref Parser::parse_block() {
    scopes_.push();
    eat(Tag::D_brace_l);
    auto res = parse_decls("block expression");
    expect(Tag::D_brace_r, "block expression");
    scopes_.pop();
    return res;
}

Ref Parser::parse_sigma() {
    auto track = tracker();
    auto bndr  = parse_tuple_ptrn(track, false, anonymous_);
    return bndr->type(world(), def2fields_);
}

Ref Parser::parse_tuple() {
    auto track = tracker();
    DefVec ops;
    parse_list("tuple", Tag::D_paren_l, [&]() { ops.emplace_back(parse_expr("tuple element")); });
    return world().tuple(ops)->set(track.loc());
}

Ref Parser::parse_type() {
    auto track = tracker();
    eat(Tag::K_Type);
    auto [l, r] = Tok::prec(Tok::Prec::App);
    auto level  = parse_expr("type level", r);
    return world().type(level)->set(track.loc());
}

Ref Parser::parse_pi() {
    auto track = tracker();
    auto tok   = lex();

    std::string name;
    switch (tok.tag()) {
        case Tag::T_Pi: name = "dependent function type"; break;
        case Tag::K_Cn: name = "continuation type"; break;
        case Tag::K_Fn: name = "returning continuation type"; break;
        default: unreachable();
    }

    Pi* first = nullptr;
    std::deque<Pi*> pis;
    scopes_.push();
    do {
        auto implicit = accept(Tag::T_dot).has_value();
        auto prec     = tok.isa(Tag::K_Cn) ? Tok::Prec::Bot : Tok::Prec::App;
        auto dom      = parse_ptrn(Tag::D_brckt_l, "domain of a "s + name, prec);
        auto dom_t    = dom->type(world(), def2fields_);
        auto pi       = world().mut_pi(world().type_infer_univ(), implicit)->set_dom(dom_t);
        auto var      = pi->var()->set(dom->sym());
        first         = first ? first : pi;

        pi->set(dom->dbg());
        dom->bind(scopes_, var);
        pis.emplace_back(pi);
    } while (ahead().isa(Tag::D_brckt_l));

    Ref codom;
    switch (tok.tag()) {
        case Tag::T_Pi:
            expect(Tag::T_arrow, name);
            codom = parse_expr("codomain of a dependent function type", Tok::Prec::Arrow);
            break;
        case Tag::K_Cn: codom = world().type_bot(); break;
        case Tag::K_Fn: {
            expect(Tag::T_arrow, name);
            codom     = world().type_bot();
            auto ret  = parse_expr("domain of return continuation", Tok::Prec::Arrow);
            auto pi   = pis.back();
            auto last = world().sigma({pi->dom(), world().cn(ret)});
            pi->unset()->set_dom(last);
            break;
        }
        default: unreachable();
    }

    for (auto pi : pis | std::ranges::views::reverse) {
        pi->set_codom(codom);
        codom = pi;
    }

    first->set(track.loc());
    scopes_.pop();
    return first;
}

Lam* Parser::parse_lam(bool decl) {
    auto track    = tracker();
    auto tok      = lex();
    auto prec     = tok.isa(Tag::K_cn) || tok.isa(Tag::K_con) ? Tok::Prec::Bot : Tok::Prec::Pi;
    bool external = decl && accept(Tag::K_extern).has_value();

    std::string name;
    // clang-format off
    switch (tok.tag()) {
        case Tag::K_lm:  name = "function expression";                break;
        case Tag::K_cn:  name = "continuation expression";            break;
        case Tag::K_fn:  name = "returning continuation expression";  break;
        case Tag::K_lam: name = "function declaration";               break;
        case Tag::K_con: name = "continuation declaration";           break;
        case Tag::K_fun: name = "returning continuation declaration"; break;
        default: unreachable();
    }
    // clang-format on

    auto dbg   = decl ? parse_sym(name) : Dbg{prev(), anonymous_};
    auto outer = scopes_.curr();

    std::unique_ptr<Ptrn> dom_p;
    scopes_.push();
    std::deque<std::tuple<Pi*, Lam*, const Def*>> funs;
    do {
        const Def* filter = accept(Tag::T_bang) ? world().lit_tt() : nullptr;
        bool implicit     = accept(Tag::T_dot).has_value();
        dom_p             = parse_ptrn(Tag::D_paren_l, "domain pattern of a "s + name, prec);
        auto dom_t        = dom_p->type(world(), def2fields_);
        auto pi           = world().mut_pi(world().type_infer_univ(), implicit)->set_dom(dom_t);
        auto lam          = world().mut_lam(pi);
        auto var          = lam->var()->set(dom_p->loc(), dom_p->sym());
        dom_p->bind(scopes_, var);

        if (auto tok = accept(Tag::T_at)) {
            if (filter) error(tok->loc(), "filter already specified via '!'");
            if (accept(Tag::T_at)) {
                filter = world().lit_tt();
            } else {
                expect(Tag::D_paren_l, "opening parenthesis of a filter");
                filter = parse_expr("filter");
                expect(Tag::D_paren_r, "closing parenthesis of a filter");
            }
        }

        funs.emplace_back(std::tuple(pi, lam, filter));
    } while (!ahead().isa(Tag::T_arrow) && !ahead().isa(Tag::T_assign) &&
             !ahead().isa(Tag::T_semicolon));

    Ref codom;
    switch (tok.tag()) {
        case Tag::K_lm:
        case Tag::K_lam: {
            codom = accept(Tag::T_arrow) ? parse_expr("return type of a "s + name, Tok::Prec::Arrow)
                                              : world().mut_infer_type();
            break;
        }
        case Tag::K_cn:
        case Tag::K_con: codom = world().type_bot(); break;
        case Tag::K_fn:
        case Tag::K_fun: {
            auto& [pi, lam, filter] = funs.back();

            codom          = world().type_bot();
            auto ret_track = tracker();
            auto ret       = accept(Tag::T_arrow) ? parse_expr("return type of a "s + name, Tok::Prec::Arrow)
                                                       : world().mut_infer_type();
            auto ret_loc   = dom_p->loc() + ret_track.loc();
            auto last      = world().sigma({pi->dom(), world().cn(ret)});
            auto new_pi    = world().mut_pi(pi->type(), pi->is_implicit())->set(ret_loc)->set_dom(last);
            auto new_lam   = world().mut_lam(new_pi);
            auto new_var   = new_lam->var()->set(ret_loc);

            if (filter) {
                // Rewrite filter - it may still use the old var.
                // First, connect old filter to lam. Otherwise, scope will not find it.
                lam->set_filter(filter);
                Scope scope(lam);

                // Now rewrite old filter to use new_var.
                ScopeRewriter rw(world(), scope);
                rw.map(lam->var(), new_lam->var(2, 0)->set(lam->var()->dbg()));
                auto new_filter = rw.rewrite(filter);
                filter          = new_filter;
            }

            lam->unset();
            pi  = new_pi;
            lam = new_lam;

            dom_p->bind(scopes_, new_var->proj(2, 0), true);
            scopes_.bind({ret_loc, return_}, new_var->proj(2, 1)->set(Dbg{ret_loc, return_}));
            break;
        }
        default: unreachable();
    }

    auto [_, first, __] = funs.front();
    first->set(dbg.sym);
    if (external) first->make_external();

    for (auto [pi, lam, _] : funs | std::ranges::views::reverse) {
        // First, connect old codom to lam. Otherwise, scope will not find it.
        pi->set_codom(codom);
        Scope scope(lam);
        ScopeRewriter rw(world(), scope);
        rw.map(lam->var(), pi->var()->set(lam->var()->dbg()));

        // Now update.
        auto dom = pi->dom();
        codom    = rw.rewrite(codom);
        pi->reset({dom, codom});
        codom = pi;
    }

    scopes_.bind(outer, dbg, first);

    auto body = accept(Tag::T_assign) ? parse_decls("body of a "s + name) : nullptr;
    if (!body) {
        if (!decl) error(prev(), "body of a {}", name);
        if (auto [_, __, filter] = funs.back(); filter) error(prev(), "cannot specify filter of a {}", name);
    }

    for (auto [_, lam, filter] : funs | std::ranges::views::reverse) {
        if (body) lam->set(filter ? filter : world().lit_ff(), body);
        body = lam;
    }

    if (decl) expect(Tag::T_semicolon, "end of "s + name);

    first->set(track.loc());

    scopes_.pop();
    return first;
}

Ref Parser::parse_lit() {
    auto track  = tracker();
    auto lit    = lex();
    auto [_, r] = Tok::prec(Tok::Prec::Lit);

    if (accept(Tag::T_colon)) {
        auto type = parse_expr("literal", r);

        // clang-format off
        switch (lit.tag()) {
            case Tag::L_s:
            case Tag::L_u:
            case Tag::L_r: break;
            case Tag::T_bot: return world().bot(type)->set(track.loc());
            case Tag::T_top: return world().top(type)->set(track.loc());
            default: unreachable();
        }
        // clang-format on
        return world().lit(type, lit.u())->set(track.loc());
    }

    if (lit.tag() == Tag::T_bot) return world().bot(world().type())->set(track.loc());
    if (lit.tag() == Tag::T_top) return world().top(world().type())->set(track.loc());
    if (lit.tag() == Tag::L_s) error(prev(), ".Nat literal specified as signed but must be unsigned");
    if (lit.tag() == Tag::L_r) error(prev(), ".Nat literal specified as floating-point but must be unsigned");

    return world().lit_nat(lit.u())->set(track.loc());
}

/*
 * ptrns
 */

std::unique_ptr<Ptrn> Parser::parse_ptrn(Tag delim_l, std::string_view ctxt, Tok::Prec prec /*= Tok::Prec::Bot*/) {
    auto track = tracker();
    auto sym   = anonymous_;
    bool p     = delim_l == Tag::D_paren_l;
    bool b     = delim_l == Tag::D_brckt_l;
    assert((p ^ b) && "left delimiter must either be '(' or '['");
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
        return parse_tuple_ptrn(track, false, sym);
    } else if (ahead().isa(Tag::D_brckt_l)) {
        // p ->    [b, ..., b]      b ->    [b, ..., b]
        return parse_tuple_ptrn(track, false, sym);
    }

    auto apos   = accept(Tag::T_apos);
    bool rebind = apos.has_value();

    if (ahead(0).isa(Tag::M_id)) {
        // p ->  s::(p, ..., p)
        // p ->  s::[b, ..., b]     b ->  s::[b, ..., b]
        // p ->  s: e               b ->  s: e
        // p ->  s                  b ->    e    where e == id
        // p -> 's::(p, ..., p)
        // p -> 's::[b, ..., b]     b -> 's::[b, ..., b]
        // p -> 's: e               b -> 's: e
        // p -> 's
        if (ahead(1).isa(Tag::T_colon_colon)) {
            sym = eat(Tag::M_id).sym();
            eat(Tag::T_colon_colon);
            if (b && ahead().isa(Tag::D_paren_l))
                error(ahead().loc(), "switching from []-style patterns to ()-style patterns is not allowed");
            // b ->  s::(p, ..., p)
            // b ->  s::[b, ..., b]     b ->  s::[b, ..., b]
            // b -> 's::(p, ..., p)
            // b -> 's::[b, ..., b]     b -> 's::[b, ..., b]
            return parse_tuple_ptrn(track, rebind, sym);
        } else if (ahead(1).isa(Tag::T_colon)) {
            // p ->  s: e               b ->  s: e
            // p -> 's: e               b -> 's: e
            sym = eat(Tag::M_id).sym();
            eat(Tag::T_colon);
            auto type = parse_expr(ctxt, prec);
            return std::make_unique<IdPtrn>(track.dbg(sym), rebind, type);
        } else {
            // p ->  s                  b ->    e    where e == id
            // p -> 's
            if (p) {
                // p ->  s
                // p -> 's
                sym = eat(Tag::M_id).sym();
                return std::make_unique<IdPtrn>(track.dbg(sym), rebind, nullptr);
            } else {
                // b ->    e    where e == id
                auto type = parse_expr(ctxt, prec);
                return std::make_unique<IdPtrn>(track.dbg(sym), rebind, type);
            }
        }
    } else if (b) {
        // b ->  e    where e != id
        if (apos) error(apos->loc(), "you can only prefix identifiers with apostrophe for rebinding");
        auto type = parse_expr(ctxt, prec);
        return std::make_unique<IdPtrn>(track.dbg(sym), rebind, type);
    } else if (!ctxt.empty()) {
        // p -> ↯
        syntax_err("pattern", ctxt);
    }

    return nullptr;
}

std::unique_ptr<TuplePtrn> Parser::parse_tuple_ptrn(Tracker track, bool rebind, Sym sym) {
    auto delim_l = ahead().tag();
    bool p       = delim_l == Tag::D_paren_l;
    bool b       = delim_l == Tag::D_brckt_l;
    assert(p ^ b);

    std::deque<std::unique_ptr<Ptrn>> ptrns;
    std::vector<Infer*> infers;
    DefVec ops;

    scopes_.push();
    parse_list("tuple pattern", delim_l, [&]() {
        auto track = tracker();
        if (!ptrns.empty()) ptrns.back()->bind(scopes_, infers.back());

        if (p && ahead(0).isa(Tag::M_id) && ahead(1).isa(Tag::M_id)) {
            std::vector<Tok> sym_toks;
            while (auto tok = accept(Tag::M_id)) sym_toks.emplace_back(*tok);

            expect(Tag::T_colon, "type ascription of an identifier group within a tuple pattern");
            auto type = parse_expr("type of an identifier group within a tuple pattern");

            for (auto tok : sym_toks) {
                infers.emplace_back(world().mut_infer(type)->set(tok.dbg()));
                ops.emplace_back(type);
                ptrns.emplace_back(std::make_unique<IdPtrn>(tok.dbg(), false, type));
            }
        } else {
            auto ptrn = parse_ptrn(delim_l, "element of a tuple pattern");
            auto type = ptrn->type(world(), def2fields_);

            if (b) {
                // If we are able to parse more stuff, we got an expression instead of just a binder.
                if (auto expr = parse_infix_expr(track, type); expr != type) {
                    ptrn = std::make_unique<IdPtrn>(track.dbg(anonymous_), false, expr);
                    type = ptrn->type(world(), def2fields_);
                }
            }

            infers.emplace_back(world().mut_infer(type)->set(ptrn->sym()));
            ops.emplace_back(type);
            ptrns.emplace_back(std::move(ptrn));
        }
    });
    scopes_.pop();

    // TODO parse type
    return std::make_unique<TuplePtrn>(track.dbg(sym), rebind, std::move(ptrns), nullptr, std::move(infers));
}

/*
 * decls
 */

Ref Parser::parse_decls(std::string_view ctxt) {
    while (true) {
        // clang-format off
        switch (ahead().tag()) {
            case Tag::T_semicolon: lex();           break; // eat up stray semicolons
            case Tag::K_ax:        parse_ax();      break;
            case Tag::K_let:       parse_let();     break;
            case Tag::K_ret:       parse_ret();     break;
            case Tag::K_Sigma:
            case Tag::K_Arr:
            case Tag::K_pack:
            case Tag::K_Pi:        parse_mut();     break;
            case Tag::K_con:
            case Tag::K_fun:
            case Tag::K_lam:       parse_lam(true); break;
            case Tag::K_def:       parse_def();     break;
            default:                    return ctxt.empty() ? nullptr : parse_expr(ctxt);
        }
        // clang-format on
    }
}

void Parser::parse_ax() {
    auto track = tracker();
    eat(Tag::K_ax);
    auto ax                 = expect(Tag::M_ax, "name of an axiom");
    auto [plugin, tag, sub] = Axiom::split(world(), ax.sym());
    auto&& [info, is_new]   = driver().axiom2info(ax.sym(), plugin, tag, ax.loc());

    if (!plugin) error(ax.loc(), "invalid axiom name '{}'", ax);
    if (sub) error(ax.loc(), "definition of axiom '{}' must not have sub in tag name", ax);

    // if (plugin != bootstrapper_.plugin()) {
    //  TODO
    //  error(ax.loc(), "axiom name `{}` implies a plugin name of `{}` but input file is named `{}`", ax,
    //  info.plugin, lexer_.file());
    //}

    std::deque<std::deque<Sym>> new_subs;
    if (ahead().isa(Tag::D_paren_l)) {
        parse_list("tag list of an axiom", Tag::D_paren_l, [&]() {
            auto& aliases = new_subs.emplace_back();
            auto [_, tag] = parse_sym("tag of an axiom");
            aliases.emplace_back(tag);
            while (accept(Tag::T_assign)) {
                auto [_, alias] = parse_sym("alias of an axiom tag");
                aliases.emplace_back(alias);
            }
        });
    }

    if (!is_new && new_subs.empty() && !info.subs.empty())
        error(ax.loc(), "redeclaration of axiom '{}' without specifying new subs", ax);
    else if (!is_new && !new_subs.empty() && info.subs.empty())
        error(ax.loc(), "cannot extend subs of axiom '{}' because it was declared as a subless axiom", ax);

    auto type = parse_type_ascr("type ascription of an axiom");
    if (!is_new && info.pi != (type->isa<Pi>() != nullptr))
        error(ax.loc(), "all declarations of axiom '{}' have to be function types if any is", ax);
    info.pi = type->isa<Pi>() != nullptr;

    Sym normalizer;
    if (ahead().isa(Tag::T_comma) && ahead(1).isa(Tag::M_id)) {
        lex();
        normalizer = parse_sym("normalizer of an axiom").sym;
    }

    if (!is_new && (info.normalizer && normalizer) && info.normalizer != normalizer)
        error(ax.loc(), "all declarations of axiom '{}' must use the same normalizer name", ax);
    info.normalizer = normalizer;

    auto [curry, trip] = Axiom::infer_curry_and_trip(type);

    if (accept(Tag::T_comma)) {
        auto c = expect(Tag::L_u, "curry counter for axiom");
        if (c.u() > curry) error(c.loc(), "curry counter cannot be greater than {}", curry);
        curry = c.u();
    }

    if (accept(Tag::T_comma)) trip = expect(Tag::L_u, "trip count for axiom").u();

    plugin_t p = *Axiom::mangle(plugin);
    tag_t t    = info.tag_id;
    sub_t s    = info.subs.size();
    if (new_subs.empty()) {
        auto norm  = driver().normalizer(p, t, 0);
        auto axiom = world().axiom(norm, curry, trip, type, p, t, 0)->set(ax.loc(), ax.sym());
        scopes_.bind(ax.dbg(), axiom);
    } else {
        for (const auto& sub : new_subs) {
            auto name  = world().sym(*ax.sym() + "."s + *sub.front());
            auto norm  = driver().normalizer(p, t, s);
            auto axiom = world().axiom(norm, curry, trip, type, p, t, s)->set(track.loc(), name);
            for (auto& alias : sub) {
                auto sym = world().sym(*ax.sym() + "."s + *alias);
                scopes_.bind({prev(), sym}, axiom);
            }
            ++s;
        }
        info.subs.insert(info.subs.end(), new_subs.begin(), new_subs.end());
    }
    expect(Tag::T_semicolon, "end of an axiom");
}

void Parser::parse_let() {
    eat(Tag::K_let);
    auto ptrn = parse_ptrn(Tag::D_paren_l, "binding pattern of a let expression");
    expect(Tag::T_assign, "let expression");
    auto expr = parse_expr("right-hand side of a let expression");
    ptrn->bind(scopes_, expr);
    expect(Tag::T_semicolon, "let expression");
}

void Parser::parse_ret() {
    eat(Tag::K_ret);
    auto ptrn = parse_ptrn(Tag::D_paren_l, "binding pattern of a let expression");
    expect(Tag::T_assign, "let expression");

    Ref cn;
    if (auto tok = accept(Tag::M_id))
        cn = scopes_.find(tok->dbg());
    else
        cn = parse_expr("continuation callee of a ret expression");
    if (auto ret_pi = Pi::ret_pi(cn->type())) {
        auto arg = parse_expr("argument of ret expression");
        auto lam = world().mut_lam(ret_pi);
        ptrn->bind(scopes_, lam->var());
        expect(Tag::T_semicolon, "let expression");
        return world().app(cn, {arg, lam});
    } else {
        error(prev(), "continuation of the ret expression is not a returning continuation but has type '{}'",
              cn->type());
    }
}

void Parser::parse_mut() {
    auto track    = tracker();
    auto tag      = lex().tag();
    bool external = accept(Tag::K_extern).has_value();
    auto dbg      = parse_sym("mutable");
    auto type     = accept(Tag::T_colon) ? parse_expr("type of a mutable") : world().type();

    Def* mut;
    switch (tag) {
        case Tag::K_Sigma: {
            expect(Tag::T_comma, "mutable Sigma");
            auto arity = expect(Tag::L_u, "arity of a mutable Sigma");
            mut        = world().mut_sigma(type, arity.u())->set(dbg);
            break;
        }
        case Tag::K_Arr: {
            expect(Tag::T_comma, "mutable array");
            auto shape = parse_expr("shape of a mutable array");
            mut        = world().mut_arr(type)->set(track.loc())->set_shape(shape);
            break;
        }
        case Tag::K_pack: mut = world().mut_pack(type)->set(dbg); break;
        case Tag::K_Pi: {
            expect(Tag::T_comma, "mutable Pi");
            auto dom = parse_expr("domain of a mutable Pi");
            mut      = world().mut_pi(type)->set(dbg)->set_dom(dom);
            break;
        }
        default: unreachable();
    }
    scopes_.bind(dbg, mut);

    scopes_.push();
    if (external) mut->make_external();

    scopes_.push();
    if (ahead().isa(Tag::T_assign))
        parse_def(dbg);
    else
        expect(Tag::T_semicolon, "end of a mutable");

    scopes_.pop();
    scopes_.pop();
}

void Parser::parse_def(Dbg dbg /*= {}*/) {
    if (!dbg.sym) {
        eat(Tag::K_def);
        dbg = parse_sym("mutable definition");
    }

    auto mut = scopes_.find(dbg)->as_mut();
    expect(Tag::T_assign, "mutable definition");

    size_t i = mut->isa<Arr, Pi>() ? 1 : 0; // first dependend op
    size_t n = mut->num_ops();

    if (ahead().isa(Tag::D_brace_l)) {
        scopes_.push();
        parse_list("mutable definition", Tag::D_brace_l, [&]() {
            if (i == n) error(prev(), "too many operands");
            mut->set(i++, parse_decls("operand of a mutable"));
        });
        scopes_.pop();
    } else if (n - i == 1) {
        mut->set(i, parse_decls("operand of a mutable"));
    } else {
        error(prev(), "expected operands for mutable definition");
    }

    expect(Tag::T_semicolon, "end of a mutable definition");
}

} // namespace thorin::fe
