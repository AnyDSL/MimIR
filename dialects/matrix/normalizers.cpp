#include "thorin/normalize.h"
#include "thorin/world.h"

#include "dialects/mem.h"
#include "dialects/matrix.h"

namespace thorin::matrix {

const Def* normalize_read(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world                = type->world();
    auto [mat, index]          = arg->projs<2>();

    // read(constMat a)=a
    if(auto cm = isa<constMat>(mat)) {
        auto v = cm->arg();
        return v;
    }

    return world.raw_app(callee, arg, dbg);
}


THORIN_matrix_NORMALIZER_IMPL

} // namespace thorin::mem
