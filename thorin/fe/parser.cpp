#include "thorin/fe/parser.h"

// clang-format off
#define DECL                \
         Tok::Tag::K_ax:    \
    case Tok::Tag::K_let:   \
    case Tok::Tag::K_Sigma: \
    case Tok::Tag::K_Arr:   \
    case Tok::Tag::K_pack:  \
    case Tok::Tag::K_Pi:    \
    case Tok::Tag::K_lam:   \
    case Tok::Tag::K_def
// clang-format on

namespace thorin {

Parser::Parser(World& world, std::string_view file, std::istream& istream, std::ostream* md)
    : lexer_(world, file, istream, md)
    , prev_(lexer_.loc())
    , bootstrapper_(file.substr(0, file.rfind('.'))) {
    for (size_t i = 0; i != Max_Ahead; ++i) lex();
    prev_ = Loc(file, {1, 1}, {1, 1});
    push(); // root scope
}

void Parser::bootstrap(std::ostream& h) { bootstrapper_.emit(h); }

Tok Parser::lex() {
    auto result = ahead();
    prev_       = ahead_[0].loc();
    for (size_t i = 0; i < Max_Ahead - 1; ++i) ahead_[i] = ahead_[i + 1];
    ahead_.back() = lexer_.lex();
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
    err(msg, ctxt);
    return {};
}

void Parser::err(std::string_view what, const Tok& tok, std::string_view ctxt) {
    err(tok.loc(), "expected {}, got '{}' while parsing {}", what, tok, ctxt);
}

void Parser::parse_module() {
    parse_decls(false);
    expect(Tok::Tag::M_eof, "module");
};

Sym Parser::parse_sym(std::string_view ctxt) {
    auto track = tracker();
    if (auto id = accept(Tok::Tag::M_id)) return id->sym();
    err("identifier", ctxt);
    return world().sym("<error>", world().dbg((Loc)track));
}

const Def* Parser::parse_expr(std::string_view ctxt, Tok::Prec p /*= Tok::Prec::Bottom*/) {
    auto track = tracker();
    auto lhs   = parse_primary_expr(ctxt);

    while (true) {
        // If operator in ahead has less left precedence: reduce (break).
        if (ahead().isa(Tok::Tag::T_extract)) {
            auto [l, r] = Tok::prec(Tok::Prec::Extract);
            if (l < p) break;
            lex();
            auto rhs = parse_expr("right-hand side of an extract", r);
            lhs      = world().extract(lhs, rhs, track);
        } else if (ahead().isa(Tok::Tag::T_arrow)) {
            auto [l, r] = Tok::prec(Tok::Prec::Arrow);
            if (l < p) break;
            lex();
            auto rhs = parse_expr("right-hand side of an function type", r);
            lhs      = world().pi(lhs, rhs, track);
        } else {
            auto [l, r] = Tok::prec(Tok::Prec::App);
            if (l < p) break;
            if (auto rhs = parse_expr({}, r)) {
                lhs = world().app(lhs, rhs, track); // if we can parse an expression, it's an App
            } else {
                return lhs;
            }
        }
    }

    return lhs;
}

const Def* Parser::parse_primary_expr(std::string_view ctxt) {
    // clang-format off
    switch (ahead().tag()) {
        case DECL:                  return parse_decls();
        case Tok::Tag::D_angle_l:   return parse_pack_or_arr(true);
        case Tok::Tag::D_quote_l:   return parse_pack_or_arr(false);
        case Tok::Tag::D_brace_l:   return parse_block();
        case Tok::Tag::D_bracket_l: return parse_sigma();
        case Tok::Tag::D_paren_l:   return parse_tuple();
        case Tok::Tag::K_Cn:        return parse_Cn();
        case Tok::Tag::K_Type:      return parse_type();
        case Tok::Tag::K_Bool:      lex(); return world().type_bool();
        case Tok::Tag::K_Nat:       lex(); return world().type_nat();
        case Tok::Tag::K_ff:        lex(); return world().lit_false();
        case Tok::Tag::K_tt:        lex(); return world().lit_true();
        case Tok::Tag::T_Pi:        return parse_pi();
        case Tok::Tag::T_lam:       return parse_lam();
        case Tok::Tag::T_at:        return parse_var();
        case Tok::Tag::T_star:      lex(); return world().type();
        case Tok::Tag::T_box:       lex(); return world().type<1>();
        case Tok::Tag::T_bot:
        case Tok::Tag::T_top:
        case Tok::Tag::L_s:
        case Tok::Tag::L_u:
        case Tok::Tag::L_r:         return parse_lit();
        case Tok::Tag::M_id:        return find(parse_sym());
        case Tok::Tag::M_i:         return lex().index();
        case Tok::Tag::M_ax: {
            // HACK hard-coded some built-in axioms
            auto tok = lex();
            auto s = tok.sym().to_string();
            if (s == ":Mem")      return world().type_mem();
            if (s == ":Int"     ) return world().type_int();
            if (s == ":Real"    ) return world().type_real();
            if (s == ":Wrap_add") return world().ax(Wrap::add);
            if (s == ":Wrap_sub") return world().ax(Wrap::sub);
            return find(tok.sym());
        }
        default:
            if (ctxt.empty()) return nullptr;
            err("primary expression", ctxt);
    }
    // clang-format on
    return nullptr;
}

const Def* Parser::parse_Cn() {
    auto track = tracker();
    eat(Tok::Tag::K_Cn);
    return world().cn(parse_expr("domain of a continuation type"), track);
}

const Def* Parser::parse_var() {
    auto track = tracker();
    eat(Tok::Tag::T_at);
    auto sym = parse_sym("variable");
    auto nom = find(sym)->isa_nom();
    if (!nom) err(prev_, "variable must reference a nominal");
    return nom->var(track.named(sym));
}

const Def* Parser::parse_pack_or_arr(bool pack) {
    push();
    auto track = tracker();
    // TODO get rid of "pack or array"
    eat(pack ? Tok::Tag::D_angle_l : Tok::Tag::D_quote_l);

    const Def* shape;
    // bool nom = false;
    if (auto id = accept(Tok::Tag::M_id)) {
        if (accept(Tok::Tag::T_colon)) {
            shape      = parse_expr("shape of a pack or array");
            auto infer = world().nom_infer(world().type_int(shape), id->sym(), id->loc());
            insert(id->sym(), infer);
        } else {
            shape = find(id->sym());
        }
    } else {
        shape = parse_expr("shape of a pack or array");
    }

    expect(Tok::Tag::T_semicolon, "pack or array");
    auto body = parse_expr("body of a pack or array");
    expect(pack ? Tok::Tag::D_angle_r : Tok::Tag::D_quote_r, "closing delimiter of a pack or array");
    pop();
    return world().arr(shape, body, track);
}

const Def* Parser::parse_block() {
    eat(Tok::Tag::D_brace_l);
    push();
    auto res = parse_expr("block expression");
    pop();
    expect(Tok::Tag::D_brace_r, "block expression");
    return res;
}

const Def* Parser::parse_sigma() {
    auto track = tracker();
    bool nom   = false;
    DefVec ops;
    parse_list("sigma", Tok::Tag::D_bracket_l, [&]() {
        if (auto id = accept(Tok::Tag::M_id)) {
            if (accept(Tok::Tag::T_colon)) {
                auto type  = parse_expr("type of a sigma element");
                auto infer = world().nom_infer(type, id->sym(), id->loc());
                nom        = true;
                insert(id->sym(), infer);
                ops.emplace_back(type);
            } else {
                ops.emplace_back(find(id->sym()));
            }
        } else {
            ops.emplace_back(parse_expr("element of a sigma"));
        }
    });

    if (!nom) return world().sigma(ops, track);
    auto sigma = world().nom_sigma(world().nom_infer_univ(), ops.size(), track);
    sigma->set(ops);
    return sigma;
}

const Def* Parser::parse_tuple() {
    auto track = tracker();
    DefVec ops;
    parse_list("tuple", Tok::Tag::D_paren_l, [&]() { ops.emplace_back(parse_expr("tuple element")); });
    return world().tuple(ops, track);
}

const Def* Parser::parse_type() {
    auto track = tracker();
    eat(Tok::Tag::K_Type);
    auto [l, r] = Tok::prec(Tok::Prec::App);
    auto level = parse_expr("type level", r);
    return world().type(level, track);
}

const Def* Parser::parse_pi() {
    auto track = tracker();
    eat(Tok::Tag::T_Pi);

    std::optional<Tok> id;
    const Def* dom;
    if (id = accept(Tok::Tag::M_id)) {
        if (accept(Tok::Tag::T_colon)) {
            dom = parse_expr("domain of a dependent function type", Tok::Prec::App);
        } else {
            dom = find(id->sym());
            id.reset();
        }
    } else {
        dom = parse_expr("domain of a dependent function type", Tok::Prec::App);
    }

    auto pi = world().nom_pi(world().nom_infer_univ(), dom);
    pi->set_dom(dom);
    push();
    if (id) insert(id->sym(), pi->var()); // TODO location/name
    expect(Tok::Tag::T_arrow, "dependent function type");
    auto codom = parse_expr("codomain of a dependent function type", Tok::Prec::Arrow);
    pi->set_codom(codom);
    pi->set_dbg(track);
    pop();
    return pi;
}

const Def* Parser::parse_lam() {
#if 0
    auto track = tracker();
    eat(Tok::Tag::T_lam);
    auto var = parse_sym("variable of a lambda abstraction");
    expect(Tok::Tag::T_semicolon, "lambda abstraction");
    auto type = parse_expr("type of a lambda abstraction");
#endif

    return nullptr;
}

const Def* Parser::parse_lit() {
    auto track  = tracker();
    auto lit    = lex();
    auto [_, r] = Tok::prec(Tok::Prec::Lit);

    if (accept(Tok::Tag::T_colon_colon)) {
        auto type = parse_expr("literal", r);

        const Def* meta = nullptr;
        // clang-format off
        switch (lit.tag()) {
            case Tok::Tag::L_s: meta = world().lit_nat('s'); break;
            case Tok::Tag::L_u: meta = world().lit_nat('u'); break;
            case Tok::Tag::L_r: meta = world().lit_nat('r'); break;
            case Tok::Tag::T_bot: return world().bot(type, track);
            case Tok::Tag::T_top: return world().top(type, track);
            default: unreachable();
        }
        // clang-format on
        return world().lit(type, lit.u(), track.meta(meta));
    }

    if (lit.tag() == Tok::Tag::T_bot) return world().bot(world().type(), track);
    if (lit.tag() == Tok::Tag::T_top) return world().top(world().type(), track);
    if (lit.tag() == Tok::Tag::L_s) err(prev_, ".Nat literal specified as signed but must be unsigned");
    if (lit.tag() == Tok::Tag::L_r) err(prev_, ".Nat literal specified as floating-point but must be unsigned");

    return world().lit_nat(lit.u(), track);
}

/*
 * decls
 */

const Def* Parser::parse_decls(bool expr /*= true*/) {
    while (true) {
        // clang-format off
        switch (ahead().tag()) {
            case Tok::Tag::K_ax:    parse_ax();  break;
            case Tok::Tag::K_let:   parse_let(); break;
            case Tok::Tag::K_Sigma:
            case Tok::Tag::K_Arr:
            case Tok::Tag::K_pack:
            case Tok::Tag::K_Pi:
            case Tok::Tag::K_lam:   parse_nom(); break;
            case Tok::Tag::K_def:   parse_def(); break;
            default:                return expr ? parse_expr("scpoe of a declaration") : nullptr;
        }
        // clang-format on
    }
}

void Parser::parse_ax() {
    auto track = tracker();
    eat(Tok::Tag::K_ax);
    auto& info = bootstrapper_.axioms.emplace_back();

    auto ax     = expect(Tok::Tag::M_ax, "name of an axiom");
    auto ax_str = ax.sym().to_string();

    auto dialect_and_group = Axiom::dialect_and_group(ax_str);
    if (!dialect_and_group) err(ax.loc(), "invalid axiom name '{}'", ax);
    info.dialect = dialect_and_group->first;
    info.group   = dialect_and_group->second;

    if (info.dialect != bootstrapper_.dialect()) {
        // TODO
        //err(ax.loc(), "axiom name `{}` implies a dialect name of `{}` but input file is named `{}`", ax, info.dialect,
            //lexer_.file());
    }

    if (ahead().isa(Tok::Tag::D_paren_l)) {
        parse_list("tag list of an axiom", Tok::Tag::D_paren_l, [&]() {
            auto& aliases = info.tags.emplace_back();
            aliases.emplace_back(parse_sym("tag of an axiom"));
            while (accept(Tok::Tag::T_assign)) aliases.emplace_back(parse_sym("alias of an axiom tag"));
        });
    }

    expect(Tok::Tag::T_colon, "axiom");
    auto type  = parse_expr("type of an axiom");
    auto axiom = world().axiom(type, track.named(ax.sym()));
    insert(ax.sym(), axiom);
    info.normalizer = (accept(Tok::Tag::T_comma) ? parse_sym("normalizer of an axiom") : Sym()).to_string();
    expect(Tok::Tag::T_semicolon, "end of an axiom");
}

void Parser::parse_let() {
    eat(Tok::Tag::K_let);
    auto sym = parse_sym();
    if (accept(Tok::Tag::T_colon)) {
        /*auto type = */ parse_expr("type of a let binding");
        // do sth with type
    }
    eat(Tok::Tag::T_assign);
    auto body = parse_expr("body of a let expression");
    insert(sym, body);
    eat(Tok::Tag::T_semicolon);
}

void Parser::parse_nom() {
    auto track    = tracker();
    auto tag      = lex().tag();
    bool external = accept(Tok::Tag::K_extern).has_value();
    auto sym      = parse_sym("nominal");
    auto type     = accept(Tok::Tag::T_colon) ? parse_expr("type of a nominal") : world().type();

    Def* nom;
    switch (tag) {
        case Tok::Tag::K_Sigma: {
            expect(Tok::Tag::T_comma, "nominal Sigma");
            auto arity = expect(Tok::Tag::L_u, "arity of a nominal Sigma");
            nom        = world().nom_sigma(type, arity.u(), track.named(sym));
            break;
        }
        case Tok::Tag::K_Arr: {
            expect(Tok::Tag::T_comma, "nominal array");
            auto shape = parse_expr("shape of a nominal array");
            nom        = world().nom_arr(type, track)->set_shape(shape);
            break;
        }
        case Tok::Tag::K_pack: nom = world().nom_pack(type, track.named(sym)); break;
        case Tok::Tag::K_Pi: {
            expect(Tok::Tag::T_comma, "nominal Pi");
            auto dom = parse_expr("domain of a nominal Pi");
            nom      = world().nom_pi(type, track.named(sym))->set_dom(dom);
            break;
        }
        case Tok::Tag::K_lam: {
            const Pi* pi = type->isa<Pi>();
            if (!pi) err(type->loc(), "type of lambda must be a Pi");
            nom = world().nom_lam(pi, track.named(sym));
            break;
        }
        default: unreachable();
    }

    insert(sym, nom);
    if (external) nom->make_external();
    if (ahead().isa(Tok::Tag::T_assign)) return parse_def(sym);
    expect(Tok::Tag::T_semicolon, "end of a nominal");
}

void Parser::parse_def(Sym sym /*= {}*/) {
    if (!sym) {
        eat(Tok::Tag::K_def);
        sym = parse_sym("nominal definition");
    }

    auto nom = find(sym)->as_nom();
    expect(Tok::Tag::T_assign, "nominal definition");

    size_t i = nom->first_dependend_op();
    size_t n = nom->num_ops();

    if (ahead().isa(Tok::Tag::D_brace_l)) {
        parse_list("nominal definition", Tok::Tag::D_brace_l, [&]() {
            if (i == n) err(prev_, "too many operands");
            nom->set(i++, parse_expr("operand of a nominal"));
        });
    } else if (n - i == 1) {
        nom->set(i, parse_expr("operand of a nominal"));
    } else {
        err(prev_, "expected operands for nominal definition");
    }

    nom->dump(0);
    expect(Tok::Tag::T_semicolon, "end of a nominal definition");
}

} // namespace thorin
