#ifndef THORIN_TOK_H
#define THORIN_TOK_H

#include "thorin/debug.h"

#include "thorin/util/types.h"

namespace thorin {

// clang-format off
#define THORIN_KEY(m)                   \
    m(K_in,  "nom" )                    \
    m(K_lam, "lam")                     \
    m(K_let, "let")

#define CODE(t, str) +size_t(1)
constexpr auto Num_Keys = size_t(0) THORIN_KEY(CODE);
#undef CODE

#define THORIN_LIT(m)                   \
    m(L_s,  "<signed integer literal>") \
    m(L_u,  "<integer literal>")        \
    m(L_r,  "<floating-point literal>")

#define THORIN_TOK(m)                   \
    /* misc */                          \
    m(M_eof, "<eof>")                   \
    m(M_id,  "<identifier>")            \
    /* punctuators */                   \
    m(P_colon,        ":")              \
    m(P_colon_colon,  "::")             \
    m(P_comma,        ",")              \
    m(P_dot,          ".")              \
    m(P_semicolon,    ";")              \
    m(P_assign,       "=")              \
    /* delimiters */                    \
    m(D_angle_l,      "‹")              \
    m(D_angle_r,      "›")              \
    m(D_brace_l,      "{")              \
    m(D_brace_r,      "}")              \
    m(D_bracket_l,    "[")              \
    m(D_bracket_r,    "]")              \
    m(D_paren_l,      "(")              \
    m(D_paren_r,      ")")              \
    m(D_quote_l,      "«")              \
    m(D_quote_r,      "»")              \
    /* binder */                        \
    m(B_lam,          "λ")              \
    m(B_forall,       "∀")

#define THORIN_OP(m)                    \
    m(O_pi,      "→", App,      Pi)     \
    m(O_extract, "#", Extract,  Lit)    \
    m(O_lit,     "∷", Error,    Lit)
// clang-format on

class Tok : public Streamable<Tok> {
public:
    enum class Prec { //  left    right
        Error,        //  -       -       <- If lookahead isn't a valid operator.
        Bottom,       //  Bottom  Bottom
        Pi,           //  App     Pi
        App,          //  App     Extract
        Extract,      //  Extract Lit
        Lit,          //  -       Lit
    };

    enum class Tag {
#define CODE(t, str) t,
        THORIN_KEY(CODE) THORIN_LIT(CODE) THORIN_TOK(CODE)
#undef CODE
#define CODE(t, str, prec_l, prec_r) t,
            THORIN_OP(CODE)
#undef CODE
    };

    Tok() {}
    Tok(Loc loc, Tag tag)
        : loc_(loc)
        , tag_(tag) {}
    Tok(Loc loc, Sym sym)
        : loc_(loc)
        , tag_(Tag::M_id)
        , sym_(sym) {}
    Tok(Loc loc, u64 u)
        : loc_(loc)
        , tag_(Tag::L_u)
        , u_(u) {}
    Tok(Loc loc, s64 s)
        : loc_(loc)
        , tag_(Tag::L_s)
        , u_(std::bit_cast<u64>(s)) {}
    Tok(Loc loc, r64 r)
        : loc_(loc)
        , tag_(Tag::L_r)
        , u_(std::bit_cast<r64>(r)) {}

    Loc loc() const { return loc_; }
    Tag tag() const { return tag_; }
    bool isa(Tag tag) const { return tag == tag_; }
    // clang-format off
    Sym sym() const { assert(isa(Tag::M_id)); return sym_; }
    u64 u()   const { assert(isa(Tag::L_u) || isa(Tag::L_s) || isa(Tag::L_r)); return u_; }
    // clang-format on
    Stream& stream(Stream& s) const;

    static std::string_view tag2str(Tok::Tag);
    static Prec tag2prec_l(Tag);
    static Prec tag2prec_r(Tag);
    static Tok::Tag delim_l2r(Tag tag) { return Tok::Tag(int(tag) + 1); }

private:
    Loc loc_;
    Tag tag_;
    union {
        Sym sym_;
        u64 u_;
    };
};

} // namespace thorin

#endif
