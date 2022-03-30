#include "thorin/fe/parser.h"

namespace thorin {

Parser::Parser(World& world, std::string_view file, std::istream& stream)
    : lexer_(world, file, stream)
    , prev_(lexer_.loc())
    , ahead_(lexer_.lex()) {}

Tok Parser::lex() {
    auto result = ahead();
    ahead_      = lexer_.lex();
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

void Parser::parse_module() { parse_def("world"); };

void Parser::err(std::string_view what, const Tok& tok, std::string_view ctxt) {
    errln("expected {}, got '{}' while parsing {}", what, tok, ctxt);
}

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
        // If operator in ahead has less left precedence: reduce.
        // If ahead isn't a valid infix operator, we will see Prec::Error.
        // This is less than all other prec levels.
        if (auto q = Tok::tag2prec_l(ahead().tag()); q < p) break;

        auto tag = lex().tag();
        auto rhs = parse_def("right-hand side of a binary expression", Tok::tag2prec_r(tag));

        switch (tag) {
            case Tok::Tag::O_extract: lhs = world().extract(lhs, rhs, dbg(track)); break;
            default: unreachable();
        }
        // lhs = mk_ptr<InfixExpr>(track, std::move(lhs), tag, std::move(rhs));
    }

    return lhs;
}

const Def* Parser::parse_primary_def(std::string_view ctxt) {
    switch (ahead().tag()) {
        // clang-format off
        case Tok::Tag::D_angle_l:   return parse_pack_or_array(true);
        case Tok::Tag::D_quote_l:   return parse_pack_or_array(false);
        case Tok::Tag::D_paren_l:   return parse_tuple();
        case Tok::Tag::D_bracket_l: return parse_sigma();
        case Tok::Tag::D_brace_l:   return parse_block();
        case Tok::Tag::M_id:        return parse_sym().def();
        case Tok::Tag::L_s:
        case Tok::Tag::L_u:
        case Tok::Tag::L_r:         return parse_lit();
        default:                    err("primary def", ctxt);
            // clang-format on
    }
    return nullptr;
}

const Def* Parser::parse_pack_or_array(bool pack) {
    auto track = tracker();
    // TODO get rid of "pack or array"
    eat(pack ? Tok::Tag::D_angle_l : Tok::Tag::D_quote_l);
    auto shape = parse_def("shape of a pack or array");
    expect(Tok::Tag::P_semicolon, "pack or array");
    auto body = parse_def("body of a pack or array");
    expect(pack ? Tok::Tag::D_angle_r : Tok::Tag::D_quote_r, "closing delimiter of a pack or array");
    return world().arr(shape, body, track);
}

const Def* Parser::parse_tuple() {
    auto ops = parse_list("tuple", Tok::Tag::D_paren_l, [this]() { return parse_def("tuple element"); });
    return world().tuple(ops);
}

const Def* Parser::parse_sigma() {
    auto ops = parse_list("sigma", Tok::Tag::D_bracket_l, [this]() { return parse_def("sigma element"); });
    auto s   = world().sigma(ops);
    s->dump(0);
    return s;
}

const Def* Parser::parse_block() {
    return nullptr; // TODO
}

const Def* Parser::parse_lit() {
    auto track = tracker();
    auto lit = lex();
    expect(Tok::Tag::P_colon_colon, "literal");
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

    return world().lit(type, lit.u(), world().dbg({"", track.loc(), meta}));
}

} // namespace thorin
