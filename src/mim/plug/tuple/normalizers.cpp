#include <mim/tuple.h>
#include <mim/world.h>

#include "mim/plug/tuple/tuple.h"

namespace mim::plug::tuple {

const Def* normalize_concat(const Def* type, const Def* callee, const Def* arg) {
    auto& world = type->world();
    auto [a, b] = arg->projs<2>();
    auto [n, m] = callee->as<App>()->decurry()->args<2>([](auto def) { return Lit::isa(def); });

    if (n && m) {
        auto defs = DefVec();
        for (size_t i = 0, e = *n; i != e; ++i) defs.emplace_back(a->proj(e, i));
        for (size_t i = 0, e = *m; i != e; ++i) defs.emplace_back(b->proj(e, i));
        return world.tuple(defs);
    }

    return nullptr;
}

MIM_tuple_NORMALIZER_IMPL

} // namespace mim::plug::tuple
