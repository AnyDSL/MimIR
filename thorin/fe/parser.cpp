#include "thorin/fe/parser.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <variant>

#include "thorin/check.h"
#include "thorin/def.h"
#include "thorin/driver.h"
#include "thorin/rewrite.h"

#include "thorin/util/array.h"
#include "thorin/util/sys.h"

using namespace std::string_literals;

namespace thorin {

using Tag = Tok::Tag;

/*
 * helpers
 */

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
    auto state = std::pair(prev_, ahead_);

    init();
    prev_ = Loc(path, {1, 1});

    parse_module();
    std::tie(prev_, ahead_) = state;
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

Dbg Parser::parse_id(std::string_view ctxt) {
    if (auto id = accept(Tag::M_id)) return id->dbg();
    syntax_err("identifier", ctxt);
    return {prev_, world().sym("<error>")};
}

std::pair<Dbg, bool> Parser::parse_name(std::string_view ctxt) {
    if (auto tok = accept(Tag::M_anx)) return {tok->dbg(), true};
    if (auto tok = accept(Tag::M_id)) return {tok->dbg(), false};
    syntax_err("identifier or annex name", ctxt);
    return {
        {prev_, world().sym("<error>")},
        false
    };
}

void Parser::register_annex(Dbg dbg, Ref def) {
    auto [plugin, tag, sub] = Annex::split(world(), dbg.sym);
    auto name               = world().sym("%"s + *plugin + "."s + *tag);
    auto&& [annex, is_new]  = driver().name2annex(name, plugin, tag, dbg.loc);
    plugin_t p              = *Annex::mangle(plugin);
    tag_t t                 = annex.tag_id;
    sub_t s                 = annex.subs.size();

    if (sub) {
        auto& aliases = annex.subs.emplace_back();
        aliases.emplace_back(sub);
    }

    world().register_annex(p | (t << 8) | s, def);
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
            if (auto extract = parse_extract_expr(track, lhs, p))
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
            bool is_explicit = accept(Tag::T_at).has_value();
            if (auto rhs = parse_expr({}, r)) // if we can parse an expression, it's an App
                lhs = (is_explicit ? world().app(lhs, rhs) : world().iapp(lhs, rhs))->set(track.loc());
            else
                return lhs;
        }
    }

    return lhs;
}

Ref Parser::parse_extract_expr(Tracker track, const Def* lhs, Tok::Prec p) {
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

Ref Parser::parse_insert_expr() {
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
        case Tag::D_quote_l: return parse_arr_expr();
        case Tag::D_angle_l: return parse_pack_expr();
        case Tag::D_brace_l: return parse_block_expr();
        case Tag::D_brckt_l: return parse_sigma_expr();
        case Tag::D_paren_l: return parse_tuple_expr();
        case Tag::K_Type:    return parse_type_expr();
        case Tag::K_Univ:    lex(); return world().univ();
        case Tag::K_Bool:    lex(); return world().type_bool();
        case Tag::K_Idx:     lex(); return world().type_idx();
        case Tag::K_Nat:     lex(); return world().type_nat();
        case Tag::K_ff:      lex(); return world().lit_ff();
        case Tag::K_tt:      lex(); return world().lit_tt();
        case Tag::K_Cn:
        case Tag::K_Fn:
        case Tag::T_Pi:      return parse_pi_expr();
        case Tag::K_cn:
        case Tag::K_fn:
        case Tag::T_lm:      return parse_lam();
        case Tag::T_star:    lex(); return world().type();
        case Tag::T_box:     lex(); return world().type<1>();
        case Tag::T_bot:
        case Tag::T_top:
        case Tag::L_s:
        case Tag::L_u:
        case Tag::L_f:       return parse_lit_expr();
        case Tag::L_c:       return world().lit_int(8, lex().lit_c());
        case Tag::L_i:       return lex().lit_i();
        case Tag::K_ins:     return parse_insert_expr();
        case Tag::K_ret:     return parse_ret_expr();
        case Tag::M_anx:
        case Tag::M_id:      return scopes_.find(lex().dbg());
        case Tag::M_str:     return world().tuple(lex().sym())->set(prev_);
        default:
            if (ctxt.empty()) return nullptr;
            syntax_err("primary expression", ctxt);
    }
    // clang-format on
    return nullptr;
}

Ref Parser::parse_arr_expr() {
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

Ref Parser::parse_pack_expr() {
    auto track = tracker();
    scopes_.push();
    eat(Tag::D_angle_l);

    if (ahead(0).isa(Tag::M_id) && ahead(1).isa(Tag::T_colon)) {
        auto id = eat(Tag::M_id);
        eat(Tag::T_colon);

        auto shape = parse_expr("shape of a pack");
        auto infer = world().mut_infer(world().type_idx(shape))->set(id.sym());
        scopes_.bind(id.dbg(), infer);

        expect(Tag::T_semicolon, "pack");
        auto body = parse_expr("body of a pack");
        expect(Tag::D_angle_r, "closing delimiter of a pack");
        scopes_.pop();

        auto arr  = world().arr(shape, body->type());
        auto pack = world().mut_pack(arr)->set(body);
        auto var  = pack->var();
        infer->set(var);
        Scope scope(pack);
        ScopeRewriter rw(scope);
        rw.map(infer, var);
        return pack->reset(rw.rewrite(pack->body()));
    }

    auto shape = parse_expr("shape of a pack");
    expect(Tag::T_semicolon, "pack");
    auto body = parse_expr("body of a pack");
    expect(Tag::D_angle_r, "closing delimiter of a pack");
    scopes_.pop();
    return world().pack(shape, body)->set(track.loc());
}

Ref Parser::parse_block_expr() {
    scopes_.push();
    eat(Tag::D_brace_l);
    auto res = parse_decls("block expression");
    expect(Tag::D_brace_r, "block expression");
    scopes_.pop();
    return res;
}

Ref Parser::parse_sigma_expr() {
    auto track = tracker();
    auto ptrn  = parse_tuple_ptrn(track, false, anonymous_);
    return ptrn->type(world(), def2fields_);
}

Ref Parser::parse_tuple_expr() {
    auto track = tracker();
    DefVec ops;
    parse_list("tuple", Tag::D_paren_l, [&]() { ops.emplace_back(parse_expr("tuple element")); });
    return world().tuple(ops)->set(track.loc());
}

Ref Parser::parse_type_expr() {
    auto track = tracker();
    eat(Tag::K_Type);
    auto [l, r] = Tok::prec(Tok::Prec::App);
    auto level  = parse_expr("type level", r);
    return world().type(level)->set(track.loc());
}

Pi* Parser::parse_pi_expr(Pi* outer) {
    auto track = tracker();
    auto tok   = lex();

    std::string entity;
    switch (tok.tag()) {
        case Tag::T_Pi: entity = "dependent function type"; break;
        case Tag::K_Cn: entity = "continuation type"; break;
        case Tag::K_Fn: entity = "returning continuation type"; break;
        default: fe::unreachable();
    }

    Pi* first = nullptr;
    std::deque<Pi*> pis;
    scopes_.push();
    do {
        auto implicit = accept(Tag::T_dot).has_value();
        auto prec     = tok.isa(Tag::K_Cn) ? Tok::Prec::Bot : Tok::Prec::App;
        auto dom      = parse_ptrn(Tag::D_brckt_l, "domain of a "s + entity, prec);
        auto dom_t    = dom->type(world(), def2fields_);
        auto pi       = (outer ? outer : world().mut_pi(world().type_infer_univ()))->set_dom(dom_t)->set(dom->dbg());
        auto var      = pi->var()->set(dom->sym());
        first         = first ? first : pi;

        if (implicit) pi->make_implicit();
        dom->bind(scopes_, var);
        pis.emplace_back(pi);
    } while (ahead().isa(Tag::T_dot) || ahead().isa(Tag::D_brckt_l) || ahead().isa(Tag::T_backtick)
             || (ahead(0).isa(Tag::M_id) && ahead(1).isa(Tag::T_colon_colon)));

    Ref codom;
    switch (tok.tag()) {
        case Tag::T_Pi:
            expect(Tag::T_arrow, entity);
            codom = parse_expr("codomain of a dependent function type", Tok::Prec::Arrow);
            break;
        case Tag::K_Cn: codom = world().type_bot(); break;
        case Tag::K_Fn: {
            expect(Tag::T_arrow, entity);
            codom     = world().type_bot();
            auto ret  = parse_expr("domain of return continuation", Tok::Prec::Arrow);
            auto pi   = pis.back();
            auto last = world().sigma({pi->dom(), world().cn(ret)});
            pi->unset()->set_dom(last);
            break;
        }
        default: fe::unreachable();
    }

    for (auto pi : pis | std::ranges::views::reverse) {
        pi->set_codom(codom);
        codom = pi;
    }

    first->set(track.loc());
    scopes_.pop();
    return first;
}

Lam* Parser::parse_lam(bool is_decl) {
    auto track    = tracker();
    auto tok      = lex();
    auto prec     = tok.isa(Tag::K_cn) || tok.isa(Tag::K_con) ? Tok::Prec::Bot : Tok::Prec::Pi;
    bool external = is_decl && accept(Tag::K_extern).has_value();

    std::string entity;
    // clang-format off
    switch (tok.tag()) {
        case Tag::T_lm:  entity = "function expression";                break;
        case Tag::K_cn:  entity = "continuation expression";            break;
        case Tag::K_fn:  entity = "returning continuation expression";  break;
        case Tag::K_lam: entity = "function declaration";               break;
        case Tag::K_con: entity = "continuation declaration";           break;
        case Tag::K_fun: entity = "returning continuation declaration"; break;
        default: fe::unreachable();
    }
    // clang-format on

    auto [dbg, anx] = is_decl ? parse_name(entity) : std::pair(Dbg{prev_, anonymous_}, false);
    auto outer      = scopes_.curr();
    Lam* decl       = nullptr;

    if (auto def = scopes_.query(dbg)) {
        if (auto lam = def->isa_mut<Lam>())
            decl = lam;
        else
            error(dbg.loc, "'{}' has not been declared as a function", dbg.sym);
    }

    std::unique_ptr<Ptrn> dom_p;
    scopes_.push();
    std::deque<std::tuple<Pi*, Lam*, const Def*>> funs;
    do {
        const Def* filter = accept(Tag::T_bang) ? world().lit_tt() : nullptr;
        bool implicit     = accept(Tag::T_dot).has_value();
        dom_p             = parse_ptrn(Tag::D_paren_l, "domain pattern of a "s + entity, prec);
        auto dom_t        = dom_p->type(world(), def2fields_);
        auto pi           = world().mut_pi(world().type_infer_univ(), implicit)->set_dom(dom_t);
        auto lam          = (funs.empty() && decl) ? decl : world().mut_lam(pi);
        auto var          = lam->var()->set<true>(dom_p->loc(), dom_p->sym());
        dom_p->bind(scopes_, var);

        if (auto tok = accept(Tag::T_at)) {
            if (filter) error(tok->loc(), "filter already specified via '!'");
            expect(Tag::D_paren_l, "opening parenthesis of a filter");
            filter = parse_expr("filter");
            expect(Tag::D_paren_r, "closing parenthesis of a filter");
        }

        funs.emplace_back(std::tuple(pi, lam, filter));
    } while (!ahead().isa(Tag::T_colon) && !ahead().isa(Tag::T_assign) && !ahead().isa(Tag::T_semicolon));

    Ref codom;
    switch (tok.tag()) {
        case Tag::T_lm:
        case Tag::K_lam: {
            codom = accept(Tag::T_colon) ? parse_expr("return type of a "s + entity) : world().mut_infer_type();
            break;
        }
        case Tag::K_cn:
        case Tag::K_con: codom = world().type_bot(); break;
        case Tag::K_fn:
        case Tag::K_fun: {
            auto& [pi, lam, filter] = funs.back();

            codom        = world().type_bot();
            auto track   = tracker();
            auto ret     = accept(Tag::T_colon) ? parse_expr("return type of a "s + entity) : world().mut_infer_type();
            auto ret_loc = dom_p->loc() + track.loc();
            auto sigma   = world().mut_sigma(2)->set(0, pi->dom());

            // Fix wrong dependencies:
            // Connect old filter to lam and ret to pi. Otherwise, scope will not find it.
            // 1. ret depends on lam->var() instead of sigma->var(2, 0)
            pi->set_codom(ret);
            if (filter) lam->set_filter(filter);
            Scope scope(lam);
            ScopeRewriter rw(scope);
            rw.map(lam->var(), sigma->var(2, 0));
            sigma->set(1, world().cn({rw.rewrite(ret)}));

            auto new_pi  = world().mut_pi(pi->type(), pi->is_implicit())->set(ret_loc)->set_dom(sigma);
            auto new_lam = world().mut_lam(new_pi);
            auto new_var = new_lam->var()->set(ret_loc);

            if (filter) {
                // 2. filter depends on lam->var() instead of new_lam->var(2, 0)
                ScopeRewriter rw(scope);
                rw.map(lam->var(), new_lam->var(2, 0)->set(lam->var()->dbg()));
                auto new_filter = rw.rewrite(filter);
                filter          = new_filter;
            }

            pi->unset();
            pi = new_pi;
            lam->unset();
            lam = new_lam;

            dom_p->bind(scopes_, new_var->proj(2, 0), true);
            scopes_.bind({ret_loc, return_}, new_var->proj(2, 1)->set(Dbg{ret_loc, return_}));
            break;
        }
        default: fe::unreachable();
    }

    auto [_, first, __] = funs.front();
    first->set(dbg.sym);

    for (auto [pi, lam, _] : funs | std::ranges::views::reverse) {
        // First, connect old codom to lam. Otherwise, scope will not find it.
        pi->set_codom(codom);
        Scope scope(lam);
        ScopeRewriter rw(scope);
        rw.map(lam->var(), pi->var()->set(lam->var()->dbg()));

        // Now update.
        auto dom = pi->dom();
        codom    = rw.rewrite(codom);
        pi->reset({dom, codom});
        codom = pi;
    }

    if (!decl) {
        scopes_.bind(outer, dbg, first);
        if (external) first->make_external();
    }

    auto body = accept(Tag::T_assign) ? parse_decls("body of a "s + entity) : nullptr;
    if (!body) {
        if (!is_decl) error(prev_, "body of a {}", entity);
        if (auto [_, __, filter] = funs.back(); filter) error(prev_, "cannot specify filter of a {}", entity);
    }

    // filter defaults to .tt for everything except the actual continuation of con/cn/fun/fn; here we use .ff as default
    bool last = true;
    for (auto [_, lam, filter] : funs | std::ranges::views::reverse) {
        bool is_cn
            = last
           && (tok.tag() == Tag::K_con || tok.tag() == Tag::K_cn || tok.tag() == Tag::K_fun || tok.tag() == Tag::K_fn);
        if (body) lam->set(filter ? filter : is_cn ? world().lit_ff() : world().lit_tt(), body);
        body = lam;
        last = false;
    }

    if (is_decl) expect(Tag::T_semicolon, "end of "s + entity);
    if (anx) register_annex(dbg, first);

    first->set(track.loc());

    scopes_.pop();
    return first;
}

Ref Parser::parse_ret_expr() {
    eat(Tag::K_ret);
    auto ptrn = parse_ptrn(Tag::D_paren_l, "binding pattern of a ret expression");
    expect(Tag::T_assign, "let expression");

    auto cn = parse_expr("continuation expression of a ret expression");
    expect(Tag::T_dollar, "separator of a ret expression");
    if (auto ret_pi = Pi::ret_pi(cn->type())) {
        auto arg = parse_expr("argument of ret expression");
        expect(Tag::T_semicolon, "let expression");
        auto lam = world().mut_lam(ret_pi);
        ptrn->bind(scopes_, lam->var());
        auto body = parse_decls("body of a ret expression");
        lam->set(false, body);
        return world().app(cn, {arg, lam});
    }

    error(prev_, "continuation of the ret expression is not a returning continuation but has type '{}'", cn->type());
}

Ref Parser::parse_lit_expr() {
    auto track  = tracker();
    auto tok    = lex();
    auto [_, r] = Tok::prec(Tok::Prec::Lit);

    if (accept(Tag::T_colon)) {
        auto type = parse_expr("literal", r);

        // clang-format off
        switch (tok.tag()) {
            case Tag::L_s:
            case Tag::L_u:
            case Tag::L_f: break;
            case Tag::T_bot: return world().bot(type)->set(track.loc());
            case Tag::T_top: return world().top(type)->set(track.loc());
            default: fe::unreachable();
        }
        // clang-format on
        return world().lit(type, tok.lit_u())->set(track.loc());
    }

    if (tok.tag() == Tag::T_bot) return world().bot(world().type())->set(track.loc());
    if (tok.tag() == Tag::T_top) return world().top(world().type())->set(track.loc());
    if (tok.tag() == Tag::L_s) error(prev_, ".Nat literal specified as signed but must be unsigned");
    if (tok.tag() == Tag::L_f) error(prev_, ".Nat literal specified as floating-point but must be unsigned");

    return world().lit_nat(tok.lit_u())->set(track.loc());
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

    auto backtick = accept(Tag::T_backtick);
    bool rebind   = backtick.has_value();

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
            if (ahead().isa(Tag::D_paren_l) || ahead().isa(Tag::D_brckt_l))
                return parse_tuple_ptrn(track, rebind, sym);
            else
                syntax_err("tuple pattern after '" + *sym + "::'", ctxt);
        } else if (ahead(1).isa(Tag::T_colon)) {
            // p ->  s: e               b ->  s: e
            // p -> 's: e               b -> 's: e
            sym = eat(Tag::M_id).sym();
            eat(Tag::T_colon);
            auto type = parse_expr(ctxt, prec);
            return std::make_unique<IdPtrn>(dbg(track, sym), rebind, type);
        } else {
            // p ->  s                  b ->    e    where e == id
            // p -> 's
            if (p) {
                // p ->  s
                // p -> 's
                sym = eat(Tag::M_id).sym();
                return std::make_unique<IdPtrn>(dbg(track, sym), rebind, nullptr);
            } else {
                // b ->    e    where e == id
                auto type = parse_expr(ctxt, prec);
                return std::make_unique<IdPtrn>(dbg(track, sym), rebind, type);
            }
        }
    } else if (b) {
        // b ->  e    where e != id
        if (backtick) error(backtick->loc(), "you can only prefix identifiers with backtick for rebinding");
        auto type = parse_expr(ctxt, prec);
        return std::make_unique<IdPtrn>(dbg(track, sym), rebind, type);
    } else if (!ctxt.empty()) {
        // p -> ↯
        syntax_err("pattern", ctxt);
    }

    return nullptr;
}

std::unique_ptr<TuplePtrn> Parser::parse_tuple_ptrn(Tracker track, bool rebind, Sym sym, Def* decl) {
    auto delim_l = ahead().tag();
    bool p       = delim_l == Tag::D_paren_l;
    bool b       = delim_l == Tag::D_brckt_l;
    assert(p ^ b);

    std::deque<std::unique_ptr<Ptrn>> ptrns;
    std::vector<Infer*> infers;
    DefVec ops;

    scopes_.push();
    parse_list("tuple pattern", delim_l, [&]() {
        parse_decls({});
        auto track = tracker();
        if (!ptrns.empty()) ptrns.back()->bind(scopes_, infers.back());
        std::unique_ptr<Ptrn> ptrn;

        if (ahead(0).isa(Tag::M_id) && ahead(1).isa(Tag::M_id)) {
            std::vector<Tok> sym_toks;
            while (auto tok = accept(Tag::M_id)) sym_toks.emplace_back(*tok);

            if (accept(Tag::T_colon)) { // identifier group: x y x: T
                auto type = parse_expr("type of an identifier group within a tuple pattern");

                for (size_t i = 0, e = sym_toks.size(); i != e; ++i) {
                    auto tok = sym_toks[i];
                    infers.emplace_back(world().mut_infer(type)->set(tok.dbg()));
                    ops.emplace_back(type);
                    auto ptrn = std::make_unique<IdPtrn>(tok.dbg(), false, type);
                    if (i != e - 1) ptrn->bind(scopes_, infers.back()); // last element will be bound above
                    ptrns.emplace_back(std::move(ptrn));
                }
                return;
            }

            // "x y z" is a curried app and maybe the prefix of a longer type expression
            auto lhs = scopes_.find(sym_toks.front().dbg());
            for (auto sym_tok : sym_toks | std::views::drop(1)) {
                auto rhs = scopes_.find(sym_tok.dbg());
                lhs      = world().iapp(lhs, rhs)->set(lhs->loc() + sym_tok.loc());
            }
            auto [_, r] = Tok::prec(Tok::Prec::App);
            auto expr   = parse_infix_expr(track, lhs, r);
            ptrn        = std::make_unique<IdPtrn>(dbg(track, anonymous_), false, expr);
        } else {
            ptrn      = parse_ptrn(delim_l, "element of a tuple pattern");
            auto type = ptrn->type(world(), def2fields_);

            if (b) { // If we are able to parse more stuff, we got an expression instead of just a binder.
                if (auto expr = parse_infix_expr(track, type); expr != type)
                    ptrn = std::make_unique<IdPtrn>(dbg(track, anonymous_), false, expr);
            }
        }

        auto type = ptrn->type(world(), def2fields_);
        infers.emplace_back(world().mut_infer(type)->set(ptrn->sym()));
        ops.emplace_back(type);
        ptrns.emplace_back(std::move(ptrn));
    });
    scopes_.pop();

    // TODO parse type
    return std::make_unique<TuplePtrn>(dbg(track, sym), rebind, std::move(ptrns), nullptr, std::move(infers), decl);
}

/*
 * decls
 */

Ref Parser::parse_decls(std::string_view ctxt) {
    while (true) {
        // clang-format off
        switch (ahead().tag()) {
            case Tag::T_semicolon: lex();              break; // eat up stray semicolons
            case Tag::K_ax:        parse_ax_decl();    break;
            case Tag::K_let:       parse_let_decl();   break;
            case Tag::K_Sigma:     parse_sigma_decl(); break;
            case Tag::K_Pi:        parse_pi_decl();    break;
            case Tag::K_con:
            case Tag::K_fun:
            case Tag::K_lam:       parse_lam(true);    break;
            default:               return ctxt.empty() ? nullptr : parse_expr(ctxt);
        }
        // clang-format on
    }
}

void Parser::parse_ax_decl() {
    auto track = tracker();
    eat(Tag::K_ax);
    auto dbg                = expect(Tag::M_anx, "annex name of an axiom").dbg();
    auto [plugin, tag, sub] = Annex::split(world(), dbg.sym);
    auto&& [annex, is_new]  = driver().name2annex(dbg.sym, plugin, tag, dbg.loc);

    if (!plugin) error(dbg.loc, "invalid axiom name '{}'", dbg.sym);
    if (sub) error(dbg.loc, "axiom '{}' must not have a subtag", dbg.sym);

    std::deque<std::deque<Sym>> new_subs;
    if (ahead().isa(Tag::D_paren_l)) {
        parse_list("tag list of an axiom", Tag::D_paren_l, [&]() {
            auto& aliases = new_subs.emplace_back();
            auto [_, tag] = parse_id("tag of an axiom");
            aliases.emplace_back(tag);
            while (accept(Tag::T_assign)) {
                auto [_, alias] = parse_id("alias of an axiom tag");
                aliases.emplace_back(alias);
            }
        });
    }

    if (!is_new && new_subs.empty() && !annex.subs.empty())
        error(dbg.loc, "redeclaration of axiom '{}' without specifying new subs", dbg.sym);
    else if (!is_new && !new_subs.empty() && annex.subs.empty())
        error(dbg.loc, "cannot extend subs of axiom '{}' because it was declared as a subless axiom", dbg.sym);

    auto type = parse_type_ascr("type ascription of an axiom");
    if (!is_new && annex.pi != (type->isa<Pi>() != nullptr))
        error(dbg.loc, "all declarations of annex '{}' have to be function types if any is", dbg.sym);
    annex.pi = type->isa<Pi>() != nullptr;

    Sym normalizer;
    if (ahead().isa(Tag::T_comma) && ahead(1).isa(Tag::M_id)) {
        lex();
        normalizer = parse_id("normalizer of an axiom").sym;
    }

    if (!is_new && (annex.normalizer && normalizer) && annex.normalizer != normalizer)
        error(dbg.loc, "all declarations of axiom '{}' must use the same normalizer name", dbg.sym);
    annex.normalizer = normalizer;

    auto [curry, trip] = Axiom::infer_curry_and_trip(type);

    if (accept(Tag::T_comma)) {
        auto c = expect(Tag::L_u, "curry counter for axiom");
        if (c.lit_u() > curry) error(c.loc(), "curry counter cannot be greater than {}", curry);
        curry = c.lit_u();
    }

    if (accept(Tag::T_comma)) trip = expect(Tag::L_u, "trip count for axiom").lit_u();

    plugin_t p = *Annex::mangle(plugin);
    tag_t t    = annex.tag_id;
    sub_t s    = annex.subs.size();
    if (new_subs.empty()) {
        auto norm  = driver().normalizer(p, t, 0);
        auto axiom = world().axiom(norm, curry, trip, type, p, t, 0)->set(dbg);
        world().register_annex(p | (flags_t(t) << 8_u64), axiom);
        scopes_.bind(dbg, axiom);
    } else {
        for (const auto& sub : new_subs) {
            auto name  = world().sym(*dbg.sym + "."s + *sub.front());
            auto norm  = driver().normalizer(p, t, s);
            auto axiom = world().axiom(norm, curry, trip, type, p, t, s)->set(track.loc(), name);
            world().register_annex(p | (flags_t(t) << 8_u64) | flags_t(s), axiom);
            for (auto& alias : sub) {
                auto sym = world().sym(*dbg.sym + "."s + *alias);
                scopes_.bind({prev_, sym}, axiom);
            }
            ++s;
        }
        annex.subs.insert(annex.subs.end(), new_subs.begin(), new_subs.end());
    }
    expect(Tag::T_semicolon, "end of an axiom");
}

void Parser::parse_let_decl() {
    auto track = tracker();
    eat(Tag::K_let);

    std::variant<std::unique_ptr<Ptrn>, Dbg> name;
    Ref type;
    if (auto tok = accept(Tag::M_anx)) {
        name = tok->dbg();
        type = parse_type_ascr();
    } else {
        auto ptrn = parse_ptrn(Tag::D_paren_l, "binding pattern of a let declaration");
        type      = ptrn->type(world(), def2fields_);
        name      = std::move(ptrn);
    }

    expect(Tag::T_assign, "let");
    auto value = parse_expr("value of a let declaration");

    if (type && !Check::assignable(type, value))
        error(track.loc(), "cannot assign value `{}` of type `{}` to its declared type `{}` in a let declaration",
              value, value->type(), type);

    if (auto dbg = std::get_if<Dbg>(&name)) {
        scopes_.bind(*dbg, value);
        register_annex(*dbg, value);
    } else {
        std::get<std::unique_ptr<Ptrn>>(name)->bind(scopes_, value);
    }

    expect(Tag::T_semicolon, "let declaration");
}

void Parser::parse_sigma_decl() {
    auto track = tracker();
    eat(Tag::K_Sigma);
    auto [dbg, anx] = parse_name("sigma declaration");
    auto type       = accept(Tag::T_colon) ? parse_expr("type of a sigma declaration") : world().type();
    auto arity      = std::optional<nat_t>{};
    if (accept(Tag::T_comma)) arity = expect(Tag::L_u, "arity of a sigma").lit_u();

    if (accept(Tag::T_assign)) {
        Def* decl;
        if (auto def = scopes_.query(dbg)) {
            if ((!def->isa_mut<Sigma>() && !def->isa<Infer>()) || !def->isa_lit_arity())
                error(dbg.loc, "'{}' has not been declared as a sigma", dbg.sym);
            if (!Check::alpha(def->type(), type))
                error(dbg.loc, "'{}' of type '{}' has been redeclared with a different type '{}'; here: {}", dbg.sym,
                      def->type(), type, def->loc());
            if (arity && *arity != def->as_lit_arity())
                error(dbg.loc, "sigma '{}' redeclared with a different arity '{}'; previous arity was '{}' here: {}",
                      dbg.sym, *arity, def->arity(), def->loc());
            decl = def->as_mut();
            if (decl->is_set())
                error(dbg.loc, "redefinition of '{}'; previous definition here: '{}'", dbg.sym, decl->loc());
        } else {
            decl = (arity ? (Def*)world().mut_sigma(type, *arity) : (Def*)world().mut_infer(type))->set(dbg);
            scopes_.bind(dbg, decl);
        }

        if (!ahead().isa(Tag::D_brckt_l)) syntax_err("sigma expression", "definition of a sigma declaration");

        auto ptrn = parse_tuple_ptrn(track, false, dbg.sym, decl);
        auto t    = ptrn->type(world(), def2fields_);

        assert(t->isa_mut<Sigma>());
        t->set<true>(track.loc());
        if (anx) register_annex(dbg, t);

        if (auto infer = decl->isa<Infer>()) {
            assert(infer->op() == t);
            infer->set<true>(track.loc());
            scopes_.bind(dbg, t, true); // rebind dbg to point to sigma instead of infer
        } else {
            assert(t == decl);
        }
    } else {
        auto decl = (arity ? (Def*)world().mut_sigma(type, *arity) : (Def*)world().mut_infer(type))->set(dbg);
        scopes_.bind(dbg, decl);
    }

    expect(Tag::T_semicolon, "end of a sigma declaration");
}

void Parser::parse_pi_decl() {
    auto track = tracker();
    eat(Tag::K_Pi);
    auto [dbg, anx] = parse_name("pi declaration");
    auto type       = accept(Tag::T_colon) ? parse_expr("type of a pi declaration") : world().type();

    if (accept(Tag::T_assign)) {
        Pi* pi;
        if (auto def = scopes_.query(dbg)) {
            if (auto mut = def->isa_mut<Pi>()) {
                if (!Check::alpha(mut->type(), type))
                    error(dbg.loc, "'{}' of type '{}' has been redeclared with a different type '{}'; here: {}",
                          dbg.sym, mut->type(), type, mut->loc());
                if (mut->is_set())
                    error(dbg.loc, "redefinition of '{}'; previous definition here: '{}'", dbg.sym, mut->loc());
                pi = mut;
            } else {
                error(dbg.loc, "'{}' has not been declared as a pi", dbg.sym);
            }
        } else {
            pi = world().mut_pi(type)->set(dbg);
            scopes_.bind(dbg, pi);
        }
        if (anx) register_annex(dbg, pi);

        if (!ahead().isa(Tag::T_Pi) && !ahead().isa(Tag::K_Cn) && !ahead().isa(Tag::K_Fn))
            syntax_err("pi expression", "definition of a pi declaration");

        auto other = parse_pi_expr(pi);
        assert_unused(other == pi);
        pi->set<true>(track.loc());
    } else {
        auto pi = world().mut_pi(type)->set(dbg);
        scopes_.bind(dbg, pi);
    }

    expect(Tag::T_semicolon, "end of a pi declaration");
}

} // namespace thorin
