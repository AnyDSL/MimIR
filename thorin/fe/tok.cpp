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

Stream& Tok::stream(Stream& s) const {
    if (isa(Tok::Tag::M_id) || isa(Tok::Tag::M_ax)) return s << sym();
    return s << Tok::tag2str(tag());
}

} // namespace thorin
