#include "thorin/normalize.h"
#include "thorin/world.h"

#include "dialects/mem.h"
#include "dialects/matrix.h"

namespace thorin::matrix {

const Def* normalize_read(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world                = type->world();
    return thorin::mem::type_mem(world);
    // TODO: read(constMat a)=a

    // auto [ptr, index]          = arg->projs<2>();
    // auto [pointee, addr_space] = match<Ptr, false>(ptr->type())->args<2>();

    // if (auto a = isa_lit(pointee->arity()); a && *a == 1) return ptr;
    // // TODO

    // return world.raw_app(callee, {ptr, index}, dbg);
}


THORIN_matrix_NORMALIZER_IMPL

} // namespace thorin::mem
