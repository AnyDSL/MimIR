#include "thorin/fe/ast.h"

#include "thorin/def.h"

#include "thorin/fe/binder.h"

namespace thorin::fe {

void IdPtrn::scrutinize(Binder& binder, const Def* scrutinee) const { binder.bind(sym_, scrutinee); }

void TuplePtrn::scrutinize(Binder& binder, const Def* scrutinee) const {
    size_t n = ptrns_.size();
    for (size_t i = 0; i != n; ++i) ptrns_[i]->scrutinize(binder, scrutinee->proj(n, i));
}

} // namespace thorin::fe
