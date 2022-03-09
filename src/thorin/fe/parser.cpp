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
        // If operator in lookahead has less left precedence: reduce.
        // If lookahead isn't a valid infix operator, we will see Prec::Error.
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

    return nullptr;
}

const Def* Parser::parse_primary_def(std::string_view ctxt) { return nullptr; }

} // namespace thorin
