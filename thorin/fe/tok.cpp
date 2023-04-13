#include "thorin/fe/tok.h"

#include "thorin/lam.h"
#include "thorin/tuple.h"

#include "thorin/util/assert.h"

namespace thorin::fe {

std::string_view Tok::tag2str(Tok::Tag tag) {
    switch (tag) {
#define CODE(t, str) \
    case Tok::Tag::t: return str;
        THORIN_KEY(CODE)
        THORIN_LIT(CODE)
        THORIN_TOK(CODE)
#undef CODE
        case Tag::Nil: unreachable();
    }
    unreachable();
}


/// @name std::ostream operator
///@{
std::ostream& operator<<(std::ostream& os, Tok tok) {
    if (tok.isa(Tok::Tag::M_id) || tok.isa(Tok::Tag::M_ax)) return os << tok.sym();
    return os << Tok::tag2str(tok.tag());
}
///@}

// clang-format off
fe::Tok::Prec Tok::prec(const Def* def) {
    if (def->isa<Pi     >()) return fe::Tok::Prec::Arrow;
    if (def->isa<App    >()) return fe::Tok::Prec::App;
    if (def->isa<Extract>()) return fe::Tok::Prec::Extract;
    if (def->isa<Lit    >()) return fe::Tok::Prec::Lit;
    return fe::Tok::Prec::Bot;
}
// clang-format on

} // namespace thorin::fe
