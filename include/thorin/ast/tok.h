#pragma once

#include <fe/assert.h>

#include "thorin/util/dbg.h"
#include "thorin/util/types.h"

namespace thorin {

class Def;
class Lit;

namespace ast {

// clang-format off
#define THORIN_KEY(m)                  \
    m(K_module, ".module")             \
    m(K_import, ".import")             \
    m(K_plugin, ".plugin")             \
    m(K_ax,     ".ax"    )             \
    m(K_def,    ".def"   )             \
    m(K_let,    ".let"   )             \
    m(K_ret,    ".ret"   )             \
    m(K_Nat,    ".Nat"   )             \
    m(K_Idx,    ".Idx"   )             \
    m(K_extern, ".extern")             \
    m(K_Sigma,  ".Sigma" )             \
    m(K_Type,   ".Type"  )             \
    m(K_Univ,   ".Univ"  )             \
    m(K_Cn,     ".Cn"    )             \
    m(K_Fn,     ".Fn"    )             \
    m(K_Pi,     ".Pi"    )             \
    m(K_con,    ".con"   )             \
    m(K_fun,    ".fun"   )             \
    m(K_lam,    ".lam"   )             \
    m(K_cn,     ".cn"    )             \
    m(K_fn,     ".fn"    )             \
    m(K_ff,     ".ff"    )             \
    m(K_tt,     ".tt"    )             \
    m(K_ins,    ".ins"   )             \
    m(K_i1,     ".i1"    )             \
    m(K_i8,     ".i8"    )             \
    m(K_i16,    ".i16"   )             \
    m(K_i32,    ".i32"   )             \
    m(K_i64,    ".i64"   )             \
    m(K_Bool,   ".Bool"  )             \
    m(K_I1,     ".I1"    )             \
    m(K_I8,     ".I8"    )             \
    m(K_I16,    ".I16"   )             \
    m(K_I32,    ".I32"   )             \
    m(K_I64,    ".I64"   )             \

#define CODE(t, str) + size_t(1)
constexpr auto Num_Keys = size_t(0) THORIN_KEY(CODE);
#undef CODE

#define THORIN_TOK(m)                  \
    m(EoF, "<end of file>")            \
    /* literals */                     \
    m(L_s, "<signed integer literal>") \
    m(L_u, "<integer literal>"       ) \
    m(L_i, "<index literal>"         ) \
    m(L_f, "<floating-point literal>") \
    m(L_c, "<char literal>"          ) \
    /* misc */                         \
    m(M_id,   "<identifier>")          \
    m(M_anx,  "<annex name>")          \
    m(M_str,  "<string>"    )          \
    /* delimiters */                   \
    m(D_angle_l,    "‹")               \
    m(D_angle_r,    "›")               \
    m(D_brace_l,    "{")               \
    m(D_brace_r,    "}")               \
    m(D_brckt_l,    "[")               \
    m(D_brckt_r,    "]")               \
    m(D_paren_l,    "(")               \
    m(D_paren_r,    ")")               \
    m(D_quote_l,    "«")               \
    m(D_quote_r,    "»")               \
    /* further tokens */               \
    m(T_Pi,         "Π")               \
    m(T_arrow,      "→")               \
    m(T_assign,     "=")               \
    m(T_at,         "@")               \
    m(T_backtick,   "`")               \
    m(T_bang,       "!")               \
    m(T_bot,        "⊥")               \
    m(T_top,        "⊤")               \
    m(T_box,        "□")               \
    m(T_colon,      ":")               \
    m(T_colon_colon,"::")              \
    m(T_comma,      ",")               \
    m(T_dollar,     "$")               \
    m(T_dot,        ".")               \
    m(T_extract,    "#")               \
    m(T_lm,         "λ")               \
    m(T_semicolon,  ";")               \
    m(T_star,       "*")               \

#define THORIN_SUBST(m)                \
    m(".lm",     T_lm   )              \
    m(".bot",    T_bot  )              \
    m(".top",    T_top  )              \
    m(".insert", K_ins  )              \

#define THORIN_PREC(m)                 \
    /* left     prec,    right  */     \
    m(Nil,      Bot,     Nil     )     \
    m(Nil,      Nil,     Nil     )     \
    m(Lam,      Arrow,   Arrow   )     \
    m(Nil,      Lam,     Pi      )     \
    m(Nil,      Pi,      App     )     \
    m(App,      App,     Extract )     \
    m(Extract,  Extract, Lit     )     \
    m(Nil,      Lit,     Lit     )     \

class Tok {
public:
    /// @name Precedence
    ///@{
    enum class Prec {
#define CODE(l, p, r) p,
        THORIN_PREC(CODE)
#undef CODE
    };

    static constexpr std::array<Prec, 2> prec(Prec p) {
        switch (p) {
#define CODE(l, p, r) \
            case Prec::p: return {Prec::l, Prec::r};
            default:      fe::unreachable();
        THORIN_PREC(CODE)
#undef CODE
        }
    }
    static Prec prec(const Def*);
    ///@}

    /// @name Tag
    ///@{
    enum class Tag {
        Nil,
#define CODE(t, str) t,
        THORIN_KEY(CODE) THORIN_TOK(CODE)
#undef CODE
    };

    static std::string_view tag2str(Tok::Tag);
    static constexpr Tok::Tag delim_l2r(Tag tag) { return Tok::Tag(int(tag) + 1); }
    ///@}

    // clang-format on

    Tok() {}
    Tok(Loc loc, Tag tag)
        : loc_(loc)
        , tag_(tag) {}
    Tok(Loc loc, char8_t c)
        : loc_(loc)
        , tag_(Tag::L_c)
        , c_(c) {}
    Tok(Loc loc, u64 u)
        : loc_(loc)
        , tag_(Tag::L_u)
        , u_(u) {}
    Tok(Loc loc, s64 s)
        : loc_(loc)
        , tag_(Tag::L_s)
        , u_(std::bit_cast<u64>(s)) {}
    Tok(Loc loc, f64 f)
        : loc_(loc)
        , tag_(Tag::L_f)
        , u_(std::bit_cast<u64>(f)) {}
    Tok(Loc loc, const Lit* i)
        : loc_(loc)
        , tag_(Tag::L_i)
        , i_(i) {}
    Tok(Loc loc, Tag tag, Sym sym)
        : loc_(loc)
        , tag_(tag)
        , sym_(sym) {
        assert(tag == Tag::M_id || tag == Tag::M_anx || tag == Tag::M_str);
    }

    bool isa(Tag tag) const { return tag == tag_; }
    Tag tag() const { return tag_; }
    Dbg dbg() const { return {loc(), sym()}; }
    Loc loc() const { return loc_; }
    explicit operator bool() const { return tag_ != Tag::Nil; }
    // clang-format off
    const Lit* lit_i() const { assert(isa(Tag::L_i)); return i_; }
    char8_t    lit_c() const { assert(isa(Tag::L_c)); return c_;   }
    u64        lit_u() const { assert(isa(Tag::L_u ) || isa(Tag::L_s ) || isa(Tag::L_f  )); return u_;   }
    Sym        sym()   const { assert(isa(Tag::M_anx) || isa(Tag::M_id) || isa(Tag::M_str)); return sym_; }
    // clang-format on
    friend std::ostream& operator<<(std::ostream&, Tok);
    friend std::ostream& operator<<(std::ostream& os, Tok::Tag tag) { return os << tag2str(tag); }

private:
    Loc loc_;
    Tag tag_ = Tag::Nil;
    union {
        Sym sym_;
        u64 u_;
        char8_t c_;
        const Lit* i_;
    };
};

} // namespace ast
} // namespace thorin
