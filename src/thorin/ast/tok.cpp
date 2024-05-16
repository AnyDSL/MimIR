#include "thorin/ast/tok.h"

#include <fe/assert.h>

#include "thorin/lam.h"
#include "thorin/tuple.h"

namespace thorin::ast {

const char* Tok::tag2str(Tok::Tag tag) {
    switch (tag) {
#define CODE(t, str) \
    case Tok::Tag::t: return str;
        THORIN_KEY(CODE)
        THORIN_TOK(CODE)
#undef CODE
        case Tag::Nil: fe::unreachable();
    }
    fe::unreachable();
}

std::string Tok::str() const {
    std::ostringstream oss;
    oss << *this;
    return oss.str();
}

/// @name std::ostream operator
///@{
std::ostream& operator<<(std::ostream& os, Tok tok) {
    if (tok.isa(Tok::Tag::M_anx) || tok.isa(Tok::Tag::M_id) || tok.isa(Tok::Tag::L_str)) return os << tok.sym();
    return os << Tok::tag2str(tok.tag());
}
///@}

} // namespace thorin::ast
