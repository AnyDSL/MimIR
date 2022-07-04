#include "thorin/fe/ast.h"

#include "thorin/def.h"

#include "thorin/fe/scopes.h"

namespace thorin::fe {

void IdPtrn::scrutinize(Scopes& scopes, const Def* scrutinee) const { scopes.bind(sym_, scrutinee); }

void TuplePtrn::scrutinize(Scopes& scopes, const Def* scrutinee) const {
    size_t n = ptrns_.size();
    for (size_t i = 0; i != n; ++i) ptrns_[i]->scrutinize(scopes, scrutinee->proj(n, i));
}

} // namespace thorin::fe
