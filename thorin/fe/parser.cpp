#include "thorin/fe/parser.h"

namespace thorin {

Parser::Parser(World& world, std::string_view file, std::istream& stream)
    : lexer_(world, file, stream)
    , prev_(lexer_.loc()) {
    for (size_t i = 0; i != Max_Ahead; ++i) lex();
    prev_ = Loc(file, {1, 1}, {1, 1});
    push(); // root scope
}

Tok Parser::lex() {
    auto result = ahead();
    prev_       = ahead_[0].loc();
    for (size_t i = 0; i < Max_Ahead - 1; ++i) ahead_[i] = ahead_[i + 1];
    ahead_.back() = lexer_.lex();
    return result;
}

bool Parser::accept(Tok::Tag tag) {
    if (tag != ahead().tag()) return false;
    lex();
    return true;
}

bool Parser::expect(Tok::Tag tag, std::string_view ctxt) {
    if (ahead().tag() == tag) {
        lex();
        return true;
    }

    std::string msg("'");
    msg.append(Tok::tag2str(tag)).append("'");
    err(msg, ctxt);
    return false;
}

void Parser::err(std::string_view what, const Tok& tok, std::string_view ctxt) {
    errln("{}: expected {}, got '{}' while parsing {}", tok.loc(), what, tok, ctxt);
}

void Parser::parse_module() {
    expect(Tok::Tag::K_module, "module");
    world().set_name(parse_sym("name of module").to_string());
    expect(Tok::Tag::D_brace_l, "module");
    parse_def("module");
    expect(Tok::Tag::D_brace_r, "module");
    expect(Tok::Tag::M_eof, "module");
};

Sym Parser::parse_sym(std::string_view ctxt) {
    auto track = tracker();
    if (ahead().isa(Tok::Tag::M_id)) return lex().sym();
    err("identifier", ctxt);
    return world().sym("<error>", world().dbg((Loc)track));
}

const Def* Parser::parse_def(std::string_view ctxt, Tok::Prec p /*= Tok::Prec::Bottom*/) {
    auto track = tracker();
    auto lhs   = parse_primary_def(ctxt);

    while (true) {
        // If operator in ahead has less left precedence: reduce (break).
        if (ahead().isa(Tok::Tag::T_extract)) {
            auto [l, r] = Tok::prec(Tok::Prec::Extract);
            if (l < p) break;
            lex();
            auto rhs = parse_def("right-hand side of an extract", r);
            lhs      = world().extract(lhs, rhs, track);
        } else if (ahead().isa(Tok::Tag::T_arrow)) {
            auto [l, r] = Tok::prec(Tok::Prec::Arrow);
            if (l < p) break;
            lex();
            auto rhs = parse_def("right-hand side of an function type", r);
            lhs      = world().pi(lhs, rhs, track);
            lhs->dump(0);
        } else {
            auto [l, r] = Tok::prec(Tok::Prec::App);
            if (l < p) break;
            if (auto rhs = parse_def({}, r)) {
                lhs = world().app(lhs, rhs, track); // if we can parse an expression, it's an App
            } else {
                return lhs;
            }
        }
    }

    return lhs;
}

const Def* Parser::parse_primary_def(std::string_view ctxt) {
    // clang-format off
    switch (ahead().tag()) {
        case Tok::Tag::D_angle_l:   return parse_pack_or_array(true);
        case Tok::Tag::D_quote_l:   return parse_pack_or_array(false);
        case Tok::Tag::D_brace_l:   return parse_block();
        case Tok::Tag::D_bracket_l: return parse_sigma();
        case Tok::Tag::D_paren_l:   return parse_tuple();
        case Tok::Tag::K_Nat:       lex(); return world().type_nat();
        case Tok::Tag::T_bot:       return parse_ext(false);
        case Tok::Tag::T_top:       return parse_ext(true);
        case Tok::Tag::T_Pi:        return parse_Pi();
        case Tok::Tag::T_lam:       return parse_lam();
        case Tok::Tag::T_star:      lex(); return world().type();
        case Tok::Tag::T_space:     lex(); return world().type<1>();
        case Tok::Tag::L_s:
        case Tok::Tag::L_u:
        case Tok::Tag::L_r:         return parse_lit();
        case Tok::Tag::M_i:         return lex().index();
        case Tok::Tag::M_ax: {
            // HACK hard-coded some built-in axioms
            auto tok = lex();
            auto s = tok.sym().to_string();
            if (s == ":Int"     ) return world().type_int();
            if (s == ":Real"    ) return world().type_real();
            if (s == ":Wrap_add") return world().ax(Wrap::add);
            if (s == ":Wrap_sub") return world().ax(Wrap::sub);
            assert(false && "TODO");
        }
        case Tok::Tag::M_id: {
            if (ahead(1).isa(Tok::Tag::T_assign) || ahead(1).isa(Tok::Tag::T_colon)) return parse_let();
            auto sym = parse_sym();
            if (auto def = find(sym)) return def;
            errln("symbol '{}' not found", sym);
            return nullptr;
        }
        default:
            if (ctxt.empty()) return nullptr;
            err("primary def", ctxt);
    }
    // clang-format on
    return nullptr;
}

const Def* Parser::parse_pack_or_array(bool pack) {
    auto track = tracker();
    // TODO get rid of "pack or array"
    eat(pack ? Tok::Tag::D_angle_l : Tok::Tag::D_quote_l);
    auto shape = parse_def("shape of a pack or array");
    expect(Tok::Tag::T_semicolon, "pack or array");
    auto body = parse_def("body of a pack or array");
    expect(pack ? Tok::Tag::D_angle_r : Tok::Tag::D_quote_r, "closing delimiter of a pack or array");
    return world().arr(shape, body, track);
}

const Def* Parser::parse_block() {
    eat(Tok::Tag::D_brace_l);
    auto res = parse_def("block expression");
    expect(Tok::Tag::D_brace_r, "block expression");
    return res;
}

const Def* Parser::parse_sigma() {
    auto ops = parse_list("sigma", Tok::Tag::D_bracket_l, [this]() { return parse_def("sigma element"); });
    auto s   = world().sigma(ops);
    s->dump(0);
    return s;
}

const Def* Parser::parse_tuple() {
    auto ops = parse_list("tuple", Tok::Tag::D_paren_l, [this]() { return parse_def("tuple element"); });
    auto t   = world().tuple(ops);
    t->dump(0);
    return t;
}

const Def* Parser::parse_ext(bool top) {
    auto track  = tracker();
    auto lit    = lex();
    auto [_, r] = Tok::prec(Tok::Prec::Lit);
    auto type   = accept(Tok::Tag::T_colon) ? parse_def("literal", r) : world().type();
    return world().ext(top, type, track);
}

const Def* Parser::parse_Pi() {
    auto track = tracker();
    eat(Tok::Tag::T_Pi);
    auto var = parse_sym("variable of a dependent function type");
    expect(Tok::Tag::T_colon, "domain of a dependent function type");
    auto dom = parse_def("domain of a dependent function type", Tok::Prec::App);
    expect(Tok::Tag::T_arrow, "dependent function type");
    auto pi = world().nom_pi(world().nom_infer(world().univ()));
    pi->set_dom(dom);
    push();
    insert(var, pi->var()); // TODO set location
    pi->set_codom(parse_def("codomain of a dependent function type", Tok::Prec::Arrow));
    pi->set_dbg(track);
    pop();
    pi->dump(1);
    return pi;
}

const Def* Parser::parse_lam() {
#if 0
    auto track = tracker();
    eat(Tok::Tag::T_lam);
    auto var = parse_sym("variable of a lambda abstraction");
    expect(Tok::Tag::T_semicolon, "lambda abstraction");
    auto type = parse_def("type of a lambda abstraction");
#endif

    return nullptr;
}

const Def* Parser::parse_lit() {
    auto track = tracker();
    auto lit   = lex();

    if (accept(Tok::Tag::T_colon_colon)) {
        auto type = parse_def("literal", Tok::Prec::Lit);

        const Def* meta = nullptr;
        switch (lit.tag()) {
            // clang-format off
            case Tok::Tag::L_s: meta = world().lit_nat('s'); break;
            case Tok::Tag::L_u: meta = world().lit_nat('u'); break;
            case Tok::Tag::L_r: meta = world().lit_nat('r'); break;
            default: unreachable();
            // clang-format on;
        }

        return world().lit(type, lit.u(), track.meta(meta));
    }

    if (lit.tag() == Tok::Tag::L_s)
        errln(".Nat literal specified as signed but must be unsigned");
    else if (lit.tag() == Tok::Tag::L_r)
        errln(".Nat literal specified as floating-point but must be unsigned");

    return world().lit_nat(lit.u(), track);
}

const Def* Parser::parse_let() {
    auto sym = parse_sym();
    if (accept(Tok::Tag::T_colon)) {
        /*auto type = */parse_def("type of a let binding");
        // do sth with type
    }
    eat(Tok::Tag::T_assign);
    auto body = parse_def("body of a let expression");
    insert(sym, body);
    eat(Tok::Tag::T_semicolon);
    return parse_def("argument of a let expression");
}

} // namespace thorin
