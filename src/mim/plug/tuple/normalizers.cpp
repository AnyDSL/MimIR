#include <mim/tuple.h>
#include <mim/world.h>

#include "mim/plug/tuple/tuple.h"

namespace mim::plug::tuple {

const Def* normalize_concat(const Def* type, const Def*, const Def* arg) {
    auto& world = type->world();
    auto [a, b] = arg->projs<2>();

    auto ta = a->isa<Tuple>();
    auto tb = b->isa<Tuple>();
    // auto pa = a->isa<Pack>();
    // auto pb = b->isa<Pack>();

    if (ta && tb) {
        auto defs = DefVec();
        for (size_t i = 0, e = ta->num_ops(); i != e; ++i) defs.emplace_back(ta->op(i));
        for (size_t i = 0, e = tb->num_ops(); i != e; ++i) defs.emplace_back(tb->op(i));
        return world.tuple(defs);
    }

    return nullptr;
}

MIM_tuple_NORMALIZER_IMPL

} // namespace mim::plug::tuple
