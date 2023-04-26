#include "thorin/fe/lexer.h"

#include "thorin/world.h"

using namespace std::literals;

namespace thorin::fe {

namespace {
bool issign(char32_t i) { return i == '+' || i == '-'; }
bool issubscsr(char32_t i) { return U'₀' <= i && i <= U'₉'; }
} // namespace

Lexer::Lexer(World& world, std::istream& istream, const fs::path* path /*= nullptr*/, std::ostream* md /*= nullptr*/)
    : Super(istream, path)
    , world_(world)
    , md_(md) {
#define CODE(t, str) keywords_[world.sym(str)] = Tok::Tag::t;
    THORIN_KEY(CODE)
#undef CODE

#define CODE(str, t) \
    if (Tok::Tag::t != Tok::Tag::Nil) keywords_[world.sym(str)] = Tok::Tag::t;
    THORIN_SUBST(CODE)
#undef CODE

    if (start_md())
        emit_md(true);
    else
        md_fence();
}

Tok Lexer::lex() {
    while (true) {
        if (auto cache = cache_) {
            cache_.reset();
            return *cache;
        }

        loc_.begin = ahead().pos;
        str_.clear();

#if defined(_WIN32) && !defined(NDEBUG) // isspace asserts otherwise
        if (accept_if([](int c) { return (c & ~0xFF) == 0 ? isspace(c) : false; })) continue;
#else
        if (accept_if(isspace)) continue;
#endif
        if (accept(utf8::Err)) error(loc_, "invalid UTF-8 character");
        if (accept(utf8::EoF)) return tok(Tok::Tag::M_eof);

        // clang-format off
        // delimiters
        if (accept( '(')) return tok(Tok::Tag::D_paren_l);
        if (accept( ')')) return tok(Tok::Tag::D_paren_r);
        if (accept( '[')) return tok(Tok::Tag::D_brckt_l);
        if (accept( ']')) return tok(Tok::Tag::D_brckt_r);
        if (accept( '{')) return tok(Tok::Tag::D_brace_l);
        if (accept( '}')) return tok(Tok::Tag::D_brace_r);
        if (accept(U'«')) return tok(Tok::Tag::D_quote_l);
        if (accept(U'»')) return tok(Tok::Tag::D_quote_r);
        if (accept(U'⟪')) return tok(Tok::Tag::D_quote_l);
        if (accept(U'⟫')) return tok(Tok::Tag::D_quote_r);
        if (accept(U'‹')) return tok(Tok::Tag::D_angle_l);
        if (accept(U'›')) return tok(Tok::Tag::D_angle_r);
        if (accept(U'⟨')) return tok(Tok::Tag::D_angle_l);
        if (accept(U'⟩')) return tok(Tok::Tag::D_angle_r);
        if (accept( '<')) {
            if (accept( '<')) return tok(Tok::Tag::D_quote_l);
            return tok(Tok::Tag::D_angle_l);
        }
        if (accept( '>')) {
            if (accept( '>')) return tok(Tok::Tag::D_quote_r);
            return tok(Tok::Tag::D_angle_r);
        }
        // further tokens
        if (accept('\'')) return tok(Tok::Tag::T_apos);
        if (accept(U'→')) return tok(Tok::Tag::T_arrow);
        if (accept( '@')) return tok(Tok::Tag::T_at);
        if (accept( '=')) return tok(Tok::Tag::T_assign);
        if (accept( '!')) return tok(Tok::Tag::T_bang);
        if (accept(U'⊥')) return tok(Tok::Tag::T_bot);
        if (accept(U'⊤')) return tok(Tok::Tag::T_top);
        if (accept(U'□')) return tok(Tok::Tag::T_box);
        if (accept( ',')) return tok(Tok::Tag::T_comma);
        if (accept( '#')) return tok(Tok::Tag::T_extract);
        if (accept(U'λ')) return tok(Tok::Tag::T_lm);
        if (accept(U'Π')) return tok(Tok::Tag::T_Pi);
        if (accept( ';')) return tok(Tok::Tag::T_semicolon);
        if (accept(U'★')) return tok(Tok::Tag::T_star);
        if (accept( '*')) return tok(Tok::Tag::T_star);
        if (accept( ':')) {
            if (accept( ':')) return tok(Tok::Tag::T_colon_colon);
            return tok(Tok::Tag::T_colon);
        }
        if (accept( '|')) {
            if (accept('~')) {
                if (accept('|')) return tok(Tok::Tag::T_Pi);
            }
            error(loc_, "invalid input char '{}'; maybe you wanted to use '|~|'?", str_);
            continue;
        }
        // clang-format on

        if (accept('%')) {
            if (lex_id()) return {loc(), Tok::Tag::M_ax, world().sym(str_)};
            error(loc_, "invalid axiom name '{}'", str_);
        }

        if (accept('.')) {
            if (lex_id()) {
                auto sym = world().sym(str_);
                if (auto i = keywords_.find(sym); i != keywords_.end()) return tok(i->second);
                // Split non-keyword into T_dot and M_id; M_id goes into cache_ for next lex().
                assert(!cache_.has_value());
                auto id_loc = loc();
                ++id_loc.begin.col;
                cache_.emplace(id_loc, Tok::Tag::M_id, world().sym(str_.substr(1)));
                return {loc().anew_begin(), Tok::Tag::T_dot};
            }

            if (accept_if(isdigit)) {
                parse_digits();
                parse_exp();
                return {loc_, f64(strtod(str_.c_str(), nullptr))};
            }

            return tok(Tok::Tag::T_dot);
        }

        if (lex_id()) return {loc(), Tok::Tag::M_id, world().sym(str_)};

        if (isdigit(ahead()) || issign(ahead())) {
            if (auto lit = parse_lit()) return *lit;
            continue;
        }

        if (start_md()) {
            emit_md();
            continue;
        }

        // comments
        if (accept('/')) {
            if (accept('*')) {
                eat_comments();
                continue;
            }
            if (accept('/')) {
                while (ahead() != utf8::EoF && ahead() != '\n') next();
                continue;
            }

            error({loc_.path, ahead().pos}, "invalid input char '/'; maybe you wanted to start a comment?");
            continue;
        }

        error({loc_.path, ahead().pos}, "invalid input char '{}'", (char)ahead());
        next();
    }
}

bool Lexer::lex_id() {
    if (accept_if([](int i) { return i == '_' || isalpha(i); })) {
        while (accept_if([](int i) { return i == '_' || i == '.' || isalnum(i); })) {}
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
        if (issubscsr(ahead())) {
            auto i = strtoull(str_.c_str(), nullptr, 10);
            std::string mod;
            while (issubscsr(ahead())) {
                mod += ahead() - U'₀' + '0';
                next();
            }
            auto m = strtoull(mod.c_str(), nullptr, 10);
            return Tok{loc_, world().lit_idx_mod(m, i)};
        } else if (accept('_', false)) {
            auto i = strtoull(str_.c_str(), nullptr, 10);
            str_.clear();
            if (accept_if(isdigit)) {
                parse_digits(10);
                auto m = strtoull(str_.c_str(), nullptr, 10);
                return Tok{loc_, world().lit_idx_mod(m, i)};
            } else {
                error(loc_, "stray underscore in unsigned literal");
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

    if (sign && str_.empty()) {
        error(loc_, "stray '{}'", *sign ? "-" : "+");
        return {};
    }

    if (is_float && base == 16) str_.insert(0, "0x"sv);
    if (sign && *sign) str_.insert(0, "-"sv);

    if (is_float) return Tok{loc_, f64(strtod  (str_.c_str(), nullptr      ))};
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
        if (!isdigit(ahead())) error(loc_, "exponent has no digits");
        parse_digits();
        return true;
    }

    if (base == 16) {
        error(loc_, "hexadecimal floating constants require an exponent");
        return {};
    }
    return false;
}
// clang-format on

void Lexer::eat_comments() {
    while (true) {
        while (ahead() != utf8::EoF && ahead() != '*') next();
        if (accept(utf8::EoF)) {
            error(loc_, "non-terminated multiline comment");
            return;
        }
        next();
        if (accept('/')) break;
    }
}

void Lexer::emit_md(bool start_of_file) {
    if (!start_of_file) md_fence();

    do {
        out_ = false;
        for (int i = 0; i < 3; ++i) next();
        accept(' ');
        out_ = true;

        while (ahead() != utf8::EoF && ahead() != '\n') next();
        accept('\n');
    } while (start_md());

    if (ahead() == utf8::EoF)
        out_ = false;
    else
        md_fence();
}

} // namespace thorin::fe
