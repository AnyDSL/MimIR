#include "thorin/fe/tok.h"

#include "thorin/util/assert.h"

namespace thorin {

std::string_view Tok::tag2str(Tok::Tag tag) {
    switch (tag) {
#define CODE(t, str) \
    case Tok::Tag::t: return str;
        THORIN_KEY(CODE)
        THORIN_TOK(CODE)
#undef CODE
#define CODE(t, str, prec_l, prec_r) \
    case Tag::t: return str;
        THORIN_OP(CODE)
#undef CODE
    }
    unreachable();
}

Tok::Prec Tok::tag2prec_l(Tag tag) {
    switch (tag) {
#define CODE(t, str, prec_l, prec_r) \
    case Tag::t: return Prec::prec_l;
        THORIN_OP(CODE)
#undef CODE
        default: return Prec::Error;
    }
}

Tok::Prec Tok::tag2prec_r(Tag tag) {
    switch (tag) {
#define CODE(t, str, prec_l, prec_r) \
    case Tag::t: return Prec::prec_r;
        THORIN_OP(CODE)
#undef CODE
        default: return Prec::Error;
    }
}

Stream& Tok::stream(Stream& s) const {
    if (isa(Tok::Tag::M_id)) return s << sym();
    return s << Tok::tag2str(tag());
}

} // namespace thorin
