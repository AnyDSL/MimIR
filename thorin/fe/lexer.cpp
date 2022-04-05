#include "thorin/fe/lexer.h"

#include "thorin/world.h"

using namespace std::literals;

namespace thorin {

static bool issign(char32_t i) { return i == '+' || i == '-'; }
static bool issubscsr(char32_t i) { return U'₀' <= i && i <= U'₉'; }

Lexer::Lexer(World& world, std::string_view filename, std::istream& stream)
    : world_(world)
    , loc_{filename, {1, 1}, {1, 1}}
    , peek_({0, Pos(1, 0)})
    , stream_(stream) {
    next();            // fill peek
    accept(utf8::BOM); // eat utf-8 BOM if present

#define CODE(t, str) keywords_[str] = Tok::Tag::t;
    THORIN_KEY(CODE)
#undef CODE

#define CODE(str, t) \
    if (Tok::Tag::t != Tok::Tag::Nil) keywords_[str] = Tok::Tag::t;
    THORIN_SUBST(CODE)
#undef CODE
}

void Lexer::next() {
    for (bool ok = true; true; ok = true) {
        // char32_t result = peek_.c32;
        peek_.c32  = stream_.get();
        loc_.finis = peek_.pos;

        if (eof()) return;

        switch (auto n = utf8::num_bytes(peek_.c32)) {
            case 0: ok = false; break;
            case 1: /*do nothing*/ break;
            default:
                peek_.c32 = utf8::first(peek_.c32, n);

                for (size_t i = 1; ok && i != n; ++i) {
                    if (auto x = utf8::is_valid(stream_.get()))
                        peek_.c32 = utf8::append(peek_.c32, *x);
                    else
                        ok = false;
                }
        }

        if (peek_.c32 == '\n') {
            ++peek_.pos.row;
            peek_.pos.col = 0;
        } else {
            ++peek_.pos.col;
        }

        if (ok) return;
        errln("{}, invalid UTF-8 character", peek_.pos);
    }
}

Tok Lexer::lex() {
    while (true) {
        loc_.begin = peek_.pos;
        str_.clear();

        if (eof()) return tok(Tok::Tag::M_eof);
        if (accept_if(isspace)) continue;

        // clang-format off
        // delimiters
        if (accept( '(')) return tok(Tok::Tag::D_paren_l);
        if (accept( ')')) return tok(Tok::Tag::D_paren_r);
        if (accept( '[')) return tok(Tok::Tag::D_bracket_l);
        if (accept( ']')) return tok(Tok::Tag::D_bracket_r);
        if (accept( '{')) return tok(Tok::Tag::D_brace_l);
        if (accept( '}')) return tok(Tok::Tag::D_brace_r);
        if (accept(U'«')) return tok(Tok::Tag::D_quote_l);
        if (accept(U'»')) return tok(Tok::Tag::D_quote_r);
        if (accept(U'‹')) return tok(Tok::Tag::D_angle_l);
        if (accept(U'›')) return tok(Tok::Tag::D_angle_r);
        if (accept( '<')) {
            if (accept( '<')) return tok(Tok::Tag::D_quote_l);
            return tok(Tok::Tag::D_angle_l);
        }
        if (accept( '>')) {
            if (accept( '>')) return tok(Tok::Tag::D_quote_r);
            return tok(Tok::Tag::D_angle_r);
        }
        // further tokens
        if (accept(U'→')) return tok(Tok::Tag::T_arrow);
        if (accept( '=')) return tok(Tok::Tag::T_assign);
        if (accept(U'⊥')) return tok(Tok::Tag::T_bot);
        if (accept(U'⊤')) return tok(Tok::Tag::T_top);
        if (accept(U'∷')) return tok(Tok::Tag::T_colon_colon);
        if (accept( ',')) return tok(Tok::Tag::T_comma);
        if (accept( '#')) return tok(Tok::Tag::T_extract);
        if (accept(U'λ')) return tok(Tok::Tag::T_lam);
        if (accept(U'Π')) return tok(Tok::Tag::T_Pi);
        if (accept( ';')) return tok(Tok::Tag::T_semicolon);
        if (accept(U'□')) return tok(Tok::Tag::T_space);
        if (accept(U'★')) return tok(Tok::Tag::T_star);
        if (accept( '*')) return tok(Tok::Tag::T_star);
        // clang-format on

        if (accept(':')) {
            if (accept(':')) return tok(Tok::Tag::T_colon_colon);
            if (lex_id()) return {loc(), Tok::Tag::M_ax, world_.sym(str_, world_.dbg(loc()))};
            return tok(Tok::Tag::T_colon);
        }

        if (accept('.')) {
            if (lex_id()) {
                if (auto i = keywords_.find(str_); i != keywords_.end()) { return tok(i->second); }
                errln("{}:{}: unknown keyword '{}'", loc_.file, peek_.pos, str_);
                continue;
            }

            if (accept_if(isdigit)) {
                parse_digits();
                parse_exp();
                return {loc_, r64(strtod(str_.c_str(), nullptr))};
            }

            return tok(Tok::Tag::T_dot);
        }

        if (lex_id()) return {loc(), Tok::Tag::M_id, world_.sym(str_, world_.dbg(loc()))};

        if (isdigit(peek_.c32) || issign(peek_.c32)) {
            if (auto lit = parse_lit()) return *lit;
            continue;
        }

        // comments
        if (accept('/')) {
            if (accept('*')) {
                eat_comments();
                continue;
            }
            if (accept('/')) {
                while (!eof() && peek_.c32 != '\n') next();
                continue;
            }

            errln("{}:{}: invalid input char '/'; maybe you wanted to start a comment?", loc_.file, peek_.pos);
            continue;
        }

        errln("{}:{}: invalid input char '{}'", loc_.file, peek_.pos, (char)peek_.c32);
        next();
    }
}

bool Lexer::lex_id() {
    if (accept_if([](int i) { return i == '_' || isalpha(i); })) {
        while (accept_if([](int i) { return i == '_' || isalnum(i); })) {}
        return true;
    }
    return false;
}

// clang-format off
std::optional<Tok> Lexer::parse_lit() {
    int base = 10;
    std::optional<bool> sign;

    if (accept('+', false)) {
        sign = false;
    } else if (accept('-', false)) {
        if (accept('>')) return tok(Tok::Tag::T_arrow);
        sign = true;
    }

    // prefix starting with '0'
    if (accept('0', false)) {
        if      (accept('b', false)) base =  2;
        else if (accept('B', false)) base =  2;
        else if (accept('o', false)) base =  8;
        else if (accept('O', false)) base =  8;
        else if (accept('x', false)) base = 16;
        else if (accept('X', false)) base = 16;
    }

    parse_digits(base);

    if (!sign && base == 10) {
        if (issubscsr(peek_.c32)) {
            auto i = strtoull(str_.c_str(), nullptr, 10);
            std::string mod;
            while (issubscsr(peek_.c32)) {
                mod += peek_.c32 - U'₀' + '0';
                next();
            }
            auto m = strtoull(mod.c_str(), nullptr, 10);
            return Tok{loc_, world().lit_int_mod(m, i)};
        } else if (accept('_', false)) {
            auto i = strtoull(str_.c_str(), nullptr, 10);
            str_.clear();
            if (accept_if(isdigit)) {
                parse_digits(10);
                auto m = strtoull(str_.c_str(), nullptr, 10);
                return Tok{loc_, world().lit_int_mod(m, i)};
            } else {
                errln("{}: stray underscore in unsigned literal", loc_);
                auto i = strtoull(str_.c_str(), nullptr, 10);
                return Tok{loc_, u64(i)};
            }
        }
    }

    bool is_float = false;
    if (base == 10 || base == 16) {
        // parse fractional part
        if (accept('.')) {
            is_float = true;
            parse_digits(base);
        }

        is_float |= parse_exp(base);
    }

    if (is_float && base == 16) str_.insert(0, "0x"sv);
    if (sign && *sign) str_.insert(0, "-"sv);

    if (str_ == "+" || str_ == "-") {
        errln("{}: stray '{}'", loc_, str_);
        return {};
    }

    if (is_float) return Tok{loc_, r64(strtod  (str_.c_str(), nullptr      ))};
    if (sign)     return Tok{loc_, u64(strtoll (str_.c_str(), nullptr, base))};
    else          return Tok{loc_, u64(strtoull(str_.c_str(), nullptr, base))};
}

void Lexer::parse_digits(int base /*= 10*/) {
    switch (base) {
        case  2: while (accept_if([](int i) { return '0' <= i && i <= '1'; })) {} break;
        case  8: while (accept_if([](int i) { return '0' <= i && i <= '7'; })) {} break;
        case 10: while (accept_if(isdigit)) {} break;
        case 16: while (accept_if(isxdigit)) {} break;
        default: unreachable();
    }
};

bool Lexer::parse_exp(int base /*= 10*/) {
    if (accept_if(base == 10 ? [](int i) { return i == 'e' || i == 'E'; }
                             : [](int i) { return i == 'p' || i == 'P'; })) {
        accept_if(issign);
        if (!isdigit(peek_.c32)) errln("{}: exponent has no digits", loc_);
        parse_digits();
        return true;
    }

    if (base == 16) {
        errln("{}: hexadecimal floating constants require an exponent", loc_);
        return {};
    }
    return false;
}
// clang-format on

void Lexer::eat_comments() {
    while (true) {
        while (!eof() && peek_.c32 != '*') next();
        if (eof()) {
            errln("{}: non-terminated multiline comment", loc_);
            return;
        }
        next();
        if (accept('/')) break;
    }
}

} // namespace thorin
