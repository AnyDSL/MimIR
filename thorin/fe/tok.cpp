#include "thorin/fe/tok.h"

#include "thorin/util/assert.h"

namespace thorin {

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

std::ostream& operator<<(std::ostream& os, const Tok tok) {
    if (tok.isa(Tok::Tag::M_id) || tok.isa(Tok::Tag::M_ax)) return os << tok.sym();
    return os << Tok::tag2str(tok.tag());
}

} // namespace thorin
