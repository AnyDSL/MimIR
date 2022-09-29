#include "thorin/world.h"

#include "dialects/refly/refly.h"

namespace thorin::refly {

static_assert(sizeof(const Def*) <= sizeof(u64), "pointer doesn't fit into Lit");

static const Def* do_reify(const Def* def, const Def* dbg = {}) {
    return def->world().lit(type_code(def->world()), (u64)def, dbg);
}

static const Def* do_reflect(const Def* def) { return reinterpret_cast<const Def*>(def->as<Lit>()->get()); }

template<dbg id>
const Def* normalize_dbg(const Def*, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = arg->world();
    debug_print(arg);
    return id == dbg::perm ? world.raw_app(callee, arg, dbg) : arg;
}

const Def* normalize_reify(const Def*, const Def*, const Def* arg, const Def* dbg) { return do_reify(arg, dbg); }

const Def* normalize_reflect(const Def*, const Def*, const Def* arg, const Def*) { return do_reflect(arg); }

const Def* normalize_refine(const Def*, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world       = arg->world();
    auto [code, i, x] = arg->projs<3>();
    if (auto l = isa_lit(i)) {
        auto def = do_reflect(code);
        return do_reify(def->refine(*l, x), dbg);
    }

    return world.raw_app(callee, arg, dbg);
}

const Def* normalize_gid(const Def*, const Def*, const Def* arg, const Def*) {
    auto& world = arg->world();
    return world.lit_nat(arg->gid());
}

THORIN_refly_NORMALIZER_IMPL

} // namespace thorin::refly
