#include "thorin/world.h"

#include "dialects/refly/refly.h"

namespace thorin::refly {

using tDef = const thorin::Def*;

static_assert(sizeof(tDef) <= sizeof(u64), "pointer doesn't fit into Lit");

/// The trick is that we simply "box" the pointer of @p def inside a Lit of type `%refly.Code`.
static tDef do_reify(tDef def, tDef dbg = {}) {
    return def->world().lit(type_def(def->world()), reinterpret_cast<u64>(def), dbg);
}

/// And here we are doing the reverse to retrieve the original pointer again.
static tDef do_reflect(tDef def) { return reinterpret_cast<tDef>(def->as<Lit>()->get()); }

static tDef world_reify(thorin::World& world, tDef dbg = {}) {
    return world.lit(type_world(world), reinterpret_cast<u64>(&world), dbg);
}

static thorin::World& world_reflect(tDef def) { return *reinterpret_cast<thorin::World*>(def->as<Lit>()->get()); }

template<dbg id>
tDef normalize_dbg(tDef, tDef callee, tDef arg, tDef dbg) {
    auto& world = arg->world();
    debug_print(arg);
    return id == dbg::perm ? world.raw_app(callee, arg, dbg) : arg;
}

tDef normalize_reify(tDef, tDef, tDef arg, tDef dbg) { return do_reify(arg, dbg); }

tDef normalize_world(tDef, tDef, tDef arg, tDef dbg) { return world_reify(arg->world(), dbg); }

tDef normalize_reflect(tDef, tDef, tDef arg, tDef) { return do_reflect(arg); }

tDef normalize_refine(tDef, tDef callee, tDef arg, tDef dbg) {
    auto& world       = arg->world();
    auto [code, i, x] = arg->projs<3>();
    if (auto l = isa_lit(i)) {
        auto def = do_reflect(code);
        return do_reify(def->refine(*l, do_reflect(x)), dbg);
    }

    return world.raw_app(callee, arg, dbg);
}

tDef normalize_gid(tDef, tDef, tDef arg, tDef) { return arg->world().lit_nat(arg->gid()); }

THORIN_refly_NORMALIZER_IMPL

} // namespace thorin::refly
