#include "thorin/fe/parser.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

#include "thorin/check.h"
#include "thorin/def.h"
#include "thorin/dialects.h"
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

Parser::Parser(World& world,
               Sym file,
               std::istream& istream,
               Span<std::string> import_search_paths,
               const Normalizers* normalizers,
               std::ostream* md)
    : world_(world)
    , bootstrapper_(world.sym(std::filesystem::path{*file}.filename().replace_extension("").string()))
    , user_search_paths_(import_search_paths.begin(), import_search_paths.end())
    , normalizers_(normalizers)
    , anonymous_(world.sym("_")) {
    init(file, istream, md);
}

void Parser::init(Sym file, std::istream& istream, std::ostream* md) {
    lexers_.emplace(world(), file, istream, md);
    for (size_t i = 0; i != Max_Ahead; ++i) ahead(i) = lexer().lex();
    prev() = Loc(file, {1, 1});
}

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

std::optional<Tok> Parser::accept(Tok::Tag tag) {
    if (tag != ahead().tag()) return {};
    return lex();
}

Tok Parser::expect(Tok::Tag tag, std::string_view ctxt) {
    if (ahead().tag() == tag) return lex();

    std::string msg("'");
    msg.append(Tok::tag2str(tag)).append("'");
    syntax_err(msg, ctxt);
    return {};
}

void Parser::syntax_err(std::string_view what, const Tok& tok, std::string_view ctxt) {
    err(tok.loc(), "expected {}, got '{}' while parsing {}", what, tok, ctxt);
}

/*
 * entry points
 */

void Parser::bootstrap(std::ostream& h) { bootstrapper_.emit(h); }

void Parser::parse_module() {
    while (ahead().tag() == Tok::Tag::K_import) parse_import();

    parse_decls({});
    expect(Tok::Tag::M_eof, "module");
};

void Parser::import(Sym name) {
    if (auto [_, ins] = world().driver().imports.emplace(name); !ins) return;

    auto search_paths = get_plugin_search_paths(user_search_paths_);
    auto file_name    = *name + ".thorin";

    std::string input_path;
    for (const auto& path : search_paths) {
        auto full_path = path / file_name;

        std::error_code ignore;
        if (bool reg_file = std::filesystem::is_regular_file(full_path, ignore); reg_file && !ignore) {
            input_path = full_path.string();
            break;
        }
    }

    std::ifstream ifs(input_path);
    if (!ifs) throw std::runtime_error("could not find file '" + file_name + "'");

    auto file  = world().sym(std::move(input_path));
    auto state = state_;
    init(file, ifs);
    parse_module();
    state_ = state;
    lexers_.pop();
}

/*
 * misc
 */

void Parser::parse_import() {
    eat(Tok::Tag::K_import);
    auto name = expect(Tok::Tag::M_id, "import name");
    expect(Tok::Tag::T_semicolon, "end of import");
    import(name.sym());
}

Dbg Parser::parse_sym(std::string_view ctxt) {
    if (auto id = accept(Tok::Tag::M_id)) return {id->dbg()};
    syntax_err("identifier", ctxt);
    return {prev(), world().sym("<error>")};
}

Ref Parser::parse_type_ascr(std::string_view ctxt) {
    if (accept(Tok::Tag::T_colon)) return parse_expr(ctxt, Tok::Prec::Bot);
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
        if (ahead().isa(Tok::Tag::T_extract)) {
            if (auto extract = parse_extract(track, lhs, p))
                lhs = extract;
            else
                break;
        } else if (ahead().isa(Tok::Tag::T_arrow)) {
            auto [l, r] = Tok::prec(Tok::Prec::Arrow);
            if (l < p) break;
            lex();
            auto rhs = parse_expr("right-hand side of an function type", r);
            lhs      = world().pi(lhs, rhs)->set(track.loc());
        } else {
            auto [l, r] = Tok::prec(Tok::Prec::App);
            if (l < p) break;
            if (auto rhs = parse_expr({}, r)) { // if we can parse an expression, it's an App
                lhs = world().iapp(lhs, rhs)->set(track.loc());
            } else {
                return lhs;
            }
        }
    }

    return lhs;
}

Ref Parser::parse_extract(Tracker track, const Def* lhs, Tok::Prec p) {
    auto [l, r] = Tok::prec(Tok::Prec::Extract);
    if (l < p) return nullptr;
    lex();

    if (ahead().isa(Tok::Tag::M_id)) {
        if (auto sigma = lhs->type()->isa_nom<Sigma>()) {
            auto tok = eat(Tok::Tag::M_id);
            if (tok.sym() == '_') err(tok.loc(), "you cannot use special symbol '_' as field access");

            if (auto i = def2fields_.find(sigma); i != def2fields_.end()) {
                if (auto& fields = i->second; fields.size() == sigma->num_ops()) {
                    for (size_t i = 0, n = sigma->num_ops(); i != n; ++i) {
                        if (fields[i] == tok.sym()) return world().extract(lhs, n, i)->set(track.loc());
                    }
                }
            }
            err(tok.loc(), "could not find elemement '{}' to extract from '{}' of type '{}'", tok.sym(), lhs, sigma);
        }
    }

    auto rhs = parse_expr("right-hand side of an extract", r);
    return world().extract(lhs, rhs)->set(track.loc());
}

Ref Parser::parse_insert() {
    eat(Tok::Tag::K_ins);
    auto track = tracker();

    expect(Tok::Tag::D_paren_l, "opening paren for insert arguments");
    auto tuple = parse_expr("the tuple to insert into");
    expect(Tok::Tag::T_comma, "comma after tuple to insert into");
    auto index = parse_expr("insert index");
    expect(Tok::Tag::T_comma, "comma after insert index");
    auto value = parse_expr("insert value");
    expect(Tok::Tag::D_paren_r, "closing paren for insert arguments");

    return world().insert(tuple, index, value)->set(track.loc());
}

Ref Parser::parse_primary_expr(std::string_view ctxt) {
    // clang-format off
    switch (ahead().tag()) {
        case Tok::Tag::D_quote_l: return parse_arr();
        case Tok::Tag::D_angle_l: return parse_pack();
        case Tok::Tag::D_brace_l: return parse_block();
        case Tok::Tag::D_brckt_l: return parse_sigma();
        case Tok::Tag::D_paren_l: return parse_tuple();
        case Tok::Tag::K_Cn:      return parse_Cn();
        case Tok::Tag::K_Type:    return parse_type();
        case Tok::Tag::K_Univ:    lex(); return world().univ();
        case Tok::Tag::K_Bool:    lex(); return world().type_bool();
        case Tok::Tag::K_Idx:     lex(); return world().type_idx();
        case Tok::Tag::K_Nat:     lex(); return world().type_nat();
        case Tok::Tag::K_ff:      lex(); return world().lit_ff();
        case Tok::Tag::K_tt:      lex(); return world().lit_tt();
        case Tok::Tag::T_Pi:      return parse_pi();
        case Tok::Tag::T_at:      return parse_var();
        case Tok::Tag::K_cn:
        case Tok::Tag::K_fn:
        case Tok::Tag::T_lm:      return parse_lam();
        case Tok::Tag::T_star:    lex(); return world().type();
        case Tok::Tag::T_box:     lex(); return world().type<1>();
        case Tok::Tag::T_bot:
        case Tok::Tag::T_top:
        case Tok::Tag::L_s:
        case Tok::Tag::L_u:
        case Tok::Tag::L_r:       return parse_lit();
        case Tok::Tag::M_id:      return scopes_.find(parse_sym());
        case Tok::Tag::M_i:       return lex().index();
        case Tok::Tag::K_ins:     return parse_insert();
        case Tok::Tag::M_ax:      return scopes_.find(lex().dbg());
        default:
            if (ctxt.empty()) return nullptr;
            syntax_err("primary expression", ctxt);
    }
    // clang-format on
    return nullptr;
}

Ref Parser::parse_Cn() {
    auto track = tracker();
    eat(Tok::Tag::K_Cn);
    auto dom = parse_ptrn(Tok::Tag::D_brckt_l, "domain of a continuation type");
    return world().cn(dom->type(world(), def2fields_))->set(track.loc());
}

Ref Parser::parse_var() {
    eat(Tok::Tag::T_at);
    auto dbg = parse_sym("variable");
    auto nom = scopes_.find(dbg)->isa_nom();
    if (!nom) err(prev(), "variable must reference a nominal");
    return nom->var()->set(dbg);
}

Ref Parser::parse_arr() {
    auto track = tracker();
    scopes_.push();
    eat(Tok::Tag::D_quote_l);

    const Def* shape = nullptr;
    Arr* arr         = nullptr;
    if (ahead(0).isa(Tok::Tag::M_id) && ahead(1).isa(Tok::Tag::T_colon)) {
        auto id = eat(Tok::Tag::M_id);
        eat(Tok::Tag::T_colon);

        auto shape = parse_expr("shape of an array");
        auto type  = world().nom_infer_univ();
        arr        = world().nom_arr(type)->set_shape(shape);
        scopes_.bind(id.dbg(), arr->var()->set(id.dbg()));
    } else {
        shape = parse_expr("shape of an array");
    }

    expect(Tok::Tag::T_semicolon, "array");
    auto body = parse_expr("body of an array");
    expect(Tok::Tag::D_quote_r, "closing delimiter of an array");
    scopes_.pop();

    if (arr) return arr->set_body(body);
    return world().arr(shape, body)->set(track.loc());
}

Ref Parser::parse_pack() {
    // TODO This doesn't work. Rework this!
    auto track = tracker();
    scopes_.push();
    eat(Tok::Tag::D_angle_l);

    const Def* shape;
    // bool nom = false;
    if (ahead(0).isa(Tok::Tag::M_id) && ahead(1).isa(Tok::Tag::T_colon)) {
        auto id = eat(Tok::Tag::M_id);
        eat(Tok::Tag::T_colon);

        shape      = parse_expr("shape of a pack");
        auto infer = world().nom_infer(world().type_idx(shape))->set(id.sym());
        scopes_.bind(id.dbg(), infer);
    } else {
        shape = parse_expr("shape of a pack");
    }

    expect(Tok::Tag::T_semicolon, "pack");
    auto body = parse_expr("body of a pack");
    expect(Tok::Tag::D_angle_r, "closing delimiter of a pack");
    scopes_.pop();
    return world().pack(shape, body)->set(track.loc());
}

Ref Parser::parse_block() {
    scopes_.push();
    eat(Tok::Tag::D_brace_l);
    auto res = parse_decls("block expression");
    expect(Tok::Tag::D_brace_r, "block expression");
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
    parse_list("tuple", Tok::Tag::D_paren_l, [&]() { ops.emplace_back(parse_expr("tuple element")); });
    return world().tuple(ops)->set(track.loc());
}

Ref Parser::parse_type() {
    auto track = tracker();
    eat(Tok::Tag::K_Type);
    auto [l, r] = Tok::prec(Tok::Prec::App);
    auto level  = parse_expr("type level", r);
    return world().type(level)->set(track.loc());
}

Ref Parser::parse_pi() {
    auto track = tracker();
    eat(Tok::Tag::T_Pi);
    scopes_.push();

    Pi* first = nullptr;
    std::deque<Pi*> pis;
    do {
        auto implicit = accept(Tok::Tag::T_dot).has_value();
        auto dom      = parse_ptrn(Tok::Tag::D_brckt_l, "domain of a dependent function type", Tok::Prec::App);
        auto pi       = world().nom_pi(world().type_infer_univ(), implicit)->set_dom(dom->type(world(), def2fields_));
        auto var      = pi->var()->set(dom->sym());
        first         = first ? first : pi;
        pi->set(dom->dbg());

        dom->bind(scopes_, var);
        pis.emplace_back(pi);
    } while (!ahead().isa(Tok::Tag::T_arrow));

    expect(Tok::Tag::T_arrow, "dependent function type");
    auto codom = parse_expr("codomain of a dependent function type", Tok::Prec::Arrow);

    for (auto pi : pis | std::ranges::views::reverse) {
        pi->set_codom(codom);
        codom = pi;
    }

    first->set(track.loc());
    scopes_.pop();
    return first;
}

Ref Parser::parse_lit() {
    auto track  = tracker();
    auto lit    = lex();
    auto [_, r] = Tok::prec(Tok::Prec::Lit);

    if (accept(Tok::Tag::T_colon)) {
        auto type = parse_expr("literal", r);

        // clang-format off
        switch (lit.tag()) {
            case Tok::Tag::L_s:
            case Tok::Tag::L_u:
            case Tok::Tag::L_r: break;
            case Tok::Tag::T_bot: return world().bot(type)->set(track.loc());
            case Tok::Tag::T_top: return world().top(type)->set(track.loc());
            default: unreachable();
        }
        // clang-format on
        return world().lit(type, lit.u())->set(track.loc());
    }

    if (lit.tag() == Tok::Tag::T_bot) return world().bot(world().type())->set(track.loc());
    if (lit.tag() == Tok::Tag::T_top) return world().top(world().type())->set(track.loc());
    if (lit.tag() == Tok::Tag::L_s) err(prev(), ".Nat literal specified as signed but must be unsigned");
    if (lit.tag() == Tok::Tag::L_r) err(prev(), ".Nat literal specified as floating-point but must be unsigned");

    return world().lit_nat(lit.u())->set(track.loc());
}

/*
 * ptrns
 */

std::unique_ptr<Ptrn> Parser::parse_ptrn(Tok::Tag delim_l, std::string_view ctxt, Tok::Prec prec /*= Tok::Prec::Bot*/) {
    auto track = tracker();
    auto sym   = anonymous_;
    bool p     = delim_l == Tok::Tag::D_paren_l;
    bool b     = delim_l == Tok::Tag::D_brckt_l;
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

    if (p && ahead().isa(Tok::Tag::D_paren_l)) {
        // p ->    (p, ..., p)
        return parse_tuple_ptrn(track, false, sym);
    } else if (ahead().isa(Tok::Tag::D_brckt_l)) {
        // p ->    [b, ..., b]      b ->    [b, ..., b]
        return parse_tuple_ptrn(track, false, sym);
    }

    auto apos   = accept(Tok::Tag::T_apos);
    bool rebind = apos.has_value();

    if (ahead(0).isa(Tok::Tag::M_id)) {
        // p ->  s::(p, ..., p)
        // p ->  s::[b, ..., b]     b ->  s::[b, ..., b]
        // p ->  s: e               b ->  s: e
        // p ->  s                  b ->    e    where e == id
        // p -> 's::(p, ..., p)
        // p -> 's::[b, ..., b]     b -> 's::[b, ..., b]
        // p -> 's: e               b -> 's: e
        // p -> 's
        if (ahead(1).isa(Tok::Tag::T_colon_colon)) {
            sym = eat(Tok::Tag::M_id).sym();
            eat(Tok::Tag::T_colon_colon);
            if (b && ahead().isa(Tok::Tag::D_paren_l))
                err(ahead().loc(), "switching from []-style patterns to ()-style patterns is not allowed");
            // b ->  s::(p, ..., p)
            // b ->  s::[b, ..., b]     b ->  s::[b, ..., b]
            // b -> 's::(p, ..., p)
            // b -> 's::[b, ..., b]     b -> 's::[b, ..., b]
            return parse_tuple_ptrn(track, rebind, sym);
        } else if (ahead(1).isa(Tok::Tag::T_colon)) {
            // p ->  s: e               b ->  s: e
            // p -> 's: e               b -> 's: e
            sym = eat(Tok::Tag::M_id).sym();
            eat(Tok::Tag::T_colon);
            auto type = parse_expr(ctxt, prec);
            return std::make_unique<IdPtrn>(track.dbg(sym), rebind, type);
        } else {
            // p ->  s                  b ->    e    where e == id
            // p -> 's
            if (p) {
                // p ->  s
                // p -> 's
                sym = eat(Tok::Tag::M_id).sym();
                return std::make_unique<IdPtrn>(track.dbg(sym), rebind, nullptr);
            } else {
                // b ->    e    where e == id
                auto type = parse_expr(ctxt, prec);
                return std::make_unique<IdPtrn>(track.dbg(sym), rebind, type);
            }
        }
    } else if (b) {
        // b ->  e    where e != id
        if (apos) err(apos->loc(), "you can only prefix identifiers with apostrophe for rebinding");
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
    bool p       = delim_l == Tok::Tag::D_paren_l;
    bool b       = delim_l == Tok::Tag::D_brckt_l;
    assert(p ^ b);

    std::deque<std::unique_ptr<Ptrn>> ptrns;
    std::vector<Infer*> infers;
    DefVec ops;

    scopes_.push();
    parse_list("tuple pattern", delim_l, [&]() {
        auto track = tracker();
        if (!ptrns.empty()) ptrns.back()->bind(scopes_, infers.back());

        if (p && ahead(0).isa(Tok::Tag::M_id) && ahead(1).isa(Tok::Tag::M_id)) {
            std::vector<Tok> sym_toks;
            while (auto tok = accept(Tok::Tag::M_id)) sym_toks.emplace_back(*tok);

            expect(Tok::Tag::T_colon, "type ascription of an identifier group within a tuple pattern");
            auto type = parse_expr("type of an identifier group within a tuple pattern");

            for (auto tok : sym_toks) {
                infers.emplace_back(world().nom_infer(type)->set(tok.dbg()));
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

            infers.emplace_back(world().nom_infer(type)->set(ptrn->sym()));
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
            case Tok::Tag::T_semicolon: lex();           break; // eat up stray semicolons
            case Tok::Tag::K_ax:        parse_ax();      break;
            case Tok::Tag::K_let:       parse_let();     break;
            case Tok::Tag::K_Sigma:
            case Tok::Tag::K_Arr:
            case Tok::Tag::K_pack:
            case Tok::Tag::K_Pi:        parse_nom();     break;
            case Tok::Tag::K_con:
            case Tok::Tag::K_fun:
            case Tok::Tag::K_lam:       parse_lam(true); break;
            case Tok::Tag::K_def:       parse_def();     break;
            default:                    return ctxt.empty() ? nullptr : parse_expr(ctxt);
        }
        // clang-format on
    }
}

void Parser::parse_ax() {
    auto track = tracker();
    eat(Tok::Tag::K_ax);
    auto ax                  = expect(Tok::Tag::M_ax, "name of an axiom");
    auto [dialect, tag, sub] = Axiom::split(world(), ax.sym());

    if (!dialect) err(ax.loc(), "invalid axiom name '{}'", ax);
    if (sub) err(ax.loc(), "definition of axiom '{}' must not have sub in tag name", ax);

    auto [it, is_new] = bootstrapper_.axioms.emplace(ax.sym(), h::AxiomInfo{});
    auto& [_, info]   = *it;
    if (is_new) {
        info.dialect = dialect;
        info.tag     = tag;
        info.tag_id  = bootstrapper_.axioms.size() - 1;
    }

    if (dialect != bootstrapper_.dialect()) {
        // TODO
        // err(ax.loc(), "axiom name `{}` implies a dialect name of `{}` but input file is named `{}`", ax,
        // info.dialect, lexer_.file());
    }

    if (bootstrapper_.axioms.size() >= std::numeric_limits<tag_t>::max())
        err(ax.loc(), "exceeded maxinum number of axioms in current dialect");

    std::deque<std::deque<Sym>> new_subs;
    if (ahead().isa(Tok::Tag::D_paren_l)) {
        parse_list("tag list of an axiom", Tok::Tag::D_paren_l, [&]() {
            auto& aliases = new_subs.emplace_back();
            auto [_, tag] = parse_sym("tag of an axiom");
            aliases.emplace_back(tag);
            while (accept(Tok::Tag::T_assign)) {
                auto [_, alias] = parse_sym("alias of an axiom tag");
                aliases.emplace_back(alias);
            }
        });
    }

    if (!is_new && new_subs.empty() && !info.subs.empty())
        err(ax.loc(), "redeclaration of axiom '{}' without specifying new subs", ax);
    else if (!is_new && !new_subs.empty() && info.subs.empty())
        err(ax.loc(), "cannot extend subs of axiom '{}' which does not have subs", ax);

    auto type = parse_type_ascr("type ascription of an axiom");
    if (!is_new && info.pi != (type->isa<Pi>() != nullptr))
        err(ax.loc(), "all declarations of axiom '{}' have to be function types if any is", ax);
    info.pi = type->isa<Pi>() != nullptr;

    auto normalizer_name = accept(Tok::Tag::T_comma) ? parse_sym("normalizer of an axiom").sym : Sym();
    if (!is_new && (info.normalizer && normalizer_name) && info.normalizer != normalizer_name)
        err(ax.loc(), "all declarations of axiom '{}' must use the same normalizer name", ax);
    info.normalizer = normalizer_name;

    auto normalizer = [this](dialect_t d, tag_t t, sub_t s) -> Def::NormalizeFn {
        if (normalizers_)
            if (auto it = normalizers_->find(d | flags_t(t << 8u) | s); it != normalizers_->end()) return it->second;
        return nullptr;
    };

    auto [curry, trip] = Axiom::infer_curry_and_trip(type);

    if (accept(Tok::Tag::T_comma)) {
        auto c = expect(Tok::Tag::L_u, "curry counter for axiom");
        if (c.u() > curry) err(c.loc(), "curry counter cannot be greater than {}", curry);
        curry = c.u();
    }

    if (accept(Tok::Tag::T_comma)) trip = expect(Tok::Tag::L_u, "trip count for axiom").u();

    dialect_t d = *Axiom::mangle(dialect);
    tag_t t     = info.tag_id;
    sub_t s     = info.subs.size();
    if (new_subs.empty()) {
        auto axiom = world().axiom(normalizer(d, t, 0), curry, trip, type, d, t, 0)->set(ax.loc(), ax.sym());
        scopes_.bind(ax.dbg(), axiom);
    } else {
        for (const auto& sub : new_subs) {
            auto name  = world().sym(*ax.sym() + "."s + *sub.front());
            auto axiom = world().axiom(normalizer(d, t, s), curry, trip, type, d, t, s)->set(track.loc(), name);
            for (auto& alias : sub) {
                auto sym = world().sym(*ax.sym() + "."s + *alias);
                scopes_.bind({prev(), sym}, axiom);
            }
            ++s;
        }
        info.subs.insert(info.subs.end(), new_subs.begin(), new_subs.end());
    }
    expect(Tok::Tag::T_semicolon, "end of an axiom");
}

void Parser::parse_let() {
    eat(Tok::Tag::K_let);
    auto ptrn = parse_ptrn(Tok::Tag::D_paren_l, "binding pattern of a let expression");
    expect(Tok::Tag::T_assign, "let expression");
    auto body = parse_expr("body of a let expression");
    ptrn->bind(scopes_, body);
    expect(Tok::Tag::T_semicolon, "let expression");
}

void Parser::parse_nom() {
    auto track    = tracker();
    auto tag      = lex().tag();
    bool external = accept(Tok::Tag::K_extern).has_value();
    auto dbg      = parse_sym("nominal");
    auto type     = accept(Tok::Tag::T_colon) ? parse_expr("type of a nominal") : world().type();

    Def* nom;
    switch (tag) {
        case Tok::Tag::K_Sigma: {
            expect(Tok::Tag::T_comma, "nominal Sigma");
            auto arity = expect(Tok::Tag::L_u, "arity of a nominal Sigma");
            nom        = world().nom_sigma(type, arity.u())->set(dbg);
            break;
        }
        case Tok::Tag::K_Arr: {
            expect(Tok::Tag::T_comma, "nominal array");
            auto shape = parse_expr("shape of a nominal array");
            nom        = world().nom_arr(type)->set(track.loc())->set_shape(shape);
            break;
        }
        case Tok::Tag::K_pack: nom = world().nom_pack(type)->set(dbg); break;
        case Tok::Tag::K_Pi: {
            expect(Tok::Tag::T_comma, "nominal Pi");
            auto dom = parse_expr("domain of a nominal Pi");
            nom      = world().nom_pi(type)->set(dbg)->set_dom(dom);
            break;
        }
        default: unreachable();
    }
    scopes_.bind(dbg, nom);

    scopes_.push();
    if (external) nom->make_external();

    scopes_.push();
    if (ahead().isa(Tok::Tag::T_assign))
        parse_def(dbg);
    else
        expect(Tok::Tag::T_semicolon, "end of a nominal");

    scopes_.pop();
    scopes_.pop();
}

Lam* Parser::parse_lam(bool decl) {
    // TODO .fn/.fun
    auto track    = tracker();
    auto tok      = lex();
    bool is_cn    = tok.isa(Tok::Tag::K_cn) || tok.isa(Tok::Tag::K_con);
    auto prec     = is_cn ? Tok::Prec::Bot : Tok::Prec::Pi;
    bool external = decl && accept(Tok::Tag::K_extern).has_value();
    auto dbg      = decl ? parse_sym("nominal lambda") : Dbg{prev(), anonymous_};

    auto outer = scopes_.curr();
    scopes_.push();

    std::deque<std::pair<Pi*, Lam*>> funs;
    Lam* first = nullptr;
    do {
        const Def* filter = world().lit_bool(accept(Tok::Tag::T_bang).has_value());
        bool implicit     = accept(Tok::Tag::T_dot).has_value();
        auto dom_p        = parse_ptrn(Tok::Tag::D_paren_l, "domain pattern of a lambda", prec);
        auto dom_t        = dom_p->type(world(), def2fields_);
        auto pi           = world().nom_pi(world().type_infer_univ(), implicit)->set_dom(dom_t);
        auto lam          = world().nom_lam(pi);
        auto lam_var      = lam->var()->set(dom_p->loc(), dom_p->sym());

        if (first == nullptr) {
            first = lam;
            lam->set(dbg.sym);
            if (external) first->make_external();
        }

        dom_p->bind(scopes_, lam_var);

        if (accept(Tok::Tag::T_at)) {
            if (accept(Tok::Tag::T_at)) {
                filter = world().lit_tt();
            } else {
                expect(Tok::Tag::D_paren_l, "opening parenthesis of a filter");
                filter = parse_expr("filter");
                expect(Tok::Tag::D_paren_r, "closing parenthesis of a filter");
            }
        }
        lam->set_filter(filter);

        funs.emplace_back(std::pair(pi, lam));
    } while (!ahead().isa(Tok::Tag::T_arrow) && !ahead().isa(Tok::Tag::T_assign) &&
             !ahead().isa(Tok::Tag::T_semicolon));

    auto codom = is_cn                     ? world().type_bot()
               : accept(Tok::Tag::T_arrow) ? parse_expr("return type of a lambda", Tok::Prec::Arrow)
                                           : world().nom_infer_type();
    for (auto [pi, lam] : funs | std::ranges::views::reverse) {
        // First, connect old codom to lam. Otherwise, scope will not find it.
        pi->set_codom(codom);
        Scope scope(lam);
        ScopeRewriter rw(world(), scope);
        rw.map(lam->var(), pi->var()->set(lam->var()->dbg()));

        // Now update.
        codom = rw.rewrite(codom);
        pi->set_codom(codom);

        codom = pi;
    }

    scopes_.bind(outer, dbg, first);

    auto body = accept(Tok::Tag::T_assign) ? parse_decls("body of a lambda") : nullptr;
    if (!body) {
        if (!decl) err(prev(), "body of a lambda expression is mandatory");
        // TODO error message if filter is non .ff
        funs.back().second->unset(0);
    }

    for (auto [_, lam] : funs | std::ranges::views::reverse) {
        lam->set_body(body);
        body = lam;
    }

    if (decl) expect(Tok::Tag::T_semicolon, "end of lambda");

    first->set(track.loc());

    scopes_.pop();
    return first;
}

void Parser::parse_def(Dbg dbg /*= {}*/) {
    if (!dbg.sym) {
        eat(Tok::Tag::K_def);
        dbg = parse_sym("nominal definition");
    }

    auto nom = scopes_.find(dbg)->as_nom();
    expect(Tok::Tag::T_assign, "nominal definition");

    size_t i = nom->first_dependend_op();
    size_t n = nom->num_ops();

    if (ahead().isa(Tok::Tag::D_brace_l)) {
        scopes_.push();
        parse_list("nominal definition", Tok::Tag::D_brace_l, [&]() {
            if (i == n) err(prev(), "too many operands");
            nom->set(i++, parse_decls("operand of a nominal"));
        });
        scopes_.pop();
    } else if (n - i == 1) {
        nom->set(i, parse_decls("operand of a nominal"));
    } else {
        err(prev(), "expected operands for nominal definition");
    }

    expect(Tok::Tag::T_semicolon, "end of a nominal definition");
}

} // namespace thorin::fe
