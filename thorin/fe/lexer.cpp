#include "thorin/fe/lexer.h"

#include "thorin/world.h"

namespace thorin {

Lexer::Lexer(World& world, std::string_view filename, std::istream& stream)
    : world_(world)
    , loc_{filename, {1, 1}, {1, 1}}
    , peek_({0, Pos(1, 0)})
    , stream_(stream)
{
    next();            // fill peek
    accept(utf8::BOM); // eat utf-8 BOM if present

#define CODE(t, str) keywords_[str] = Tok::Tag::t;
    THORIN_KEY(CODE)
#undef CODE
}

char32_t Lexer::next() {
    for (bool ok = true; true; ok = true) {
        char32_t result = peek_.char_;
        peek_.char_     = stream_.get();
        loc_.finis      = peek_.pos_;

        if (eof()) return result;

        switch (auto n = utf8::num_bytes(peek_.char_)) {
            case 0: ok = false; break;
            case 1: /*do nothing*/ break;
            default:
                peek_.char_ = utf8::first(peek_.char_, n);

                for (size_t i = 1; ok && i != n; ++i) {
                    if (auto x = utf8::is_valid(stream_.get()))
                        peek_.char_ = utf8::append(peek_.char_, *x);
                    else
                        ok = false;
                }
        }

        if (peek_.char_ == '\n') {
            ++peek_.pos_.row;
            peek_.pos_.col = 0;
        } else {
            ++peek_.pos_.col;
        }

        if (ok) return result;

        errln("{}, invalid UTF-8 character", peek_.pos_);
    }
}

Tok Lexer::lex() {
    while (true) {
        loc_.begin = peek_.pos_;
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
        if (accept( '=')) return tok(Tok::Tag::P_assign);
        if (accept( ':')) return tok(Tok::Tag::P_colon);
        if (accept( ',')) return tok(Tok::Tag::P_comma);
        if (accept( '.')) return tok(Tok::Tag::P_dot);

        // binder
        if (accept(U'λ')) return tok(Tok::Tag::B_lam);
        if (accept(U'∀')) return tok(Tok::Tag::B_forall);
        if (accept('\\')) {
            if (accept('/')) return tok(Tok::Tag::B_forall);
            return tok(Tok::Tag::B_lam);
        }
        // clang-format on

        if (accept('/')) {
            if (accept('*')) {
                eat_comments();
                continue;
            }
            if (accept('/')) {
                while (!eof() && peek_.char_ != '\n') next();
                continue;
            }

            errln("{}:{}: invalid input char '/'; maybe you wanted to start a comment?", loc_.file, peek_.pos_);
            continue;
        }

        // identifier or keyword
        if (accept_if([](int i) { return i == '_' || isalpha(i); })) {
            while (accept_if([](int i) { return i == '_' || isalpha(i) || isdigit(i); })) {}
            if (auto i = keywords_.find(str_); i != keywords_.end()) return tok(i->second); // keyword
            return {loc(), world_.sym(str_, world_.dbg(loc()))};                            // identifier
        }

        errln("{}:{}: invalid input char '{}'", loc_.file, peek_.pos_, (char)peek_.char_);
        next();
    }
}

void Lexer::eat_comments() {
    while (true) {
        while (!eof() && peek_.char_ != '*') next();
        if (eof()) {
            errln("{}:{}: non-terminated multiline comment", loc_);
            return;
        }
        next();
        if (accept('/')) break;
    }
}

} // namespace thorin
