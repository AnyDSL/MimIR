#include <mim/driver.h>
#include <mim/world.h>

#include "mim/plug/compile/compile.h"

namespace mim::plug::compile {

const Def* normalize_is_loaded(const Def*, const Def*, const Def* arg) {
    auto& world  = arg->world();
    auto& driver = world.driver();
    if (auto str = tuple2str(arg); !str.empty()) return world.lit_bool(driver.is_loaded(driver.sym(str)));

    return {};
}

const Def* normalize_cat(const Def* type, const Def* callee, const Def* arg) {
    auto& world = type->world();
    auto [a, b] = arg->projs<2>();
    auto [n, m] = callee->as<App>()->args<2>([](auto def) { return Lit::isa(def); });

    if (n && *n == 0) return b;
    if (m && *m == 0) return a;

    if (n && m) {
        auto defs = DefVec();
        for (size_t i = 0, e = *n; i != e; ++i)
            defs.emplace_back(a->proj(e, i));
        for (size_t i = 0, e = *m; i != e; ++i)
            defs.emplace_back(b->proj(e, i));
        return world.tuple(defs);
    }

    return nullptr;
}

MIM_compile_NORMALIZER_IMPL

} // namespace mim::plug::compile
