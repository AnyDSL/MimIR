#include <mim/plug/core/core.h>

#include "mim/world.h"

#include "mim/plug/vec/vec.h"

namespace mim::plug::vec {

template<fold id> const Def* normalize_fold(const Def*, const Def* c, const Def* arg) {
    auto& w     = c->world();
    auto callee = c->as<App>();
    auto f      = callee->arg();

    auto [acc, vec] = arg->projs<2>();
    if constexpr (id == fold::r) std::swap(acc, vec);

    if (auto tuple = vec->isa<Tuple>()) {
        if constexpr (id == fold::l)
            for (auto op : tuple->ops()) acc = w.app(f, {acc, op});
        else // fold::r
            for (auto op : tuple->ops() | std::ranges::views::reverse) acc = w.app(f, {op, acc});
        return acc;
    }

    return nullptr;
}

template<scan id> const Def* normalize_scan(const Def*, const Def* c, const Def* vec) {
    auto& w     = c->world();
    auto callee = c->as<App>();
    auto p      = callee->arg();

    if (auto tuple = vec->isa<Tuple>()) {
        const Def* acc = w.lit_bool(id != scan::exists);
        for (auto op : tuple->ops())
            acc = w.call(id == scan::exists ? core::bit2::or_ : core::bit2::and_, 0_n, Defs{acc, w.app(p, op)});
        return acc;
    }

    return nullptr;
}

const Def* normalize_diff(const Def*, const Def* c, const Def* vec) { return nullptr; }

MIM_vec_NORMALIZER_IMPL

} // namespace mim::plug::vec
