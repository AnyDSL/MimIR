#include "thorin/world.h"

#include "dialects/refly/refly.h"

namespace thorin::refly {

static_assert(sizeof(void*) <= sizeof(u64), "pointer doesn't fit into Lit");

namespace {
// The trick is that we simply "box" the pointer of @p def inside a Lit of type `%refly.Code`.
Ref do_reify(const Def* def) { return def->world().lit(type_code(def->world()), reinterpret_cast<u64>(def)); }

// And here we are doing the reverse to retrieve the original pointer again.
const Def* do_reflect(const Def* def) { return reinterpret_cast<const Def*>(def->as<Lit>()->get()); }

// TODO: check (and fix) for windows
#define YELLOW "\033[0;33m"
#define BLANK  "\033[0m"

void debug_print(const Def* def) {
    auto& world = def->world();
    world.DLOG(YELLOW "debug_print: {}" BLANK, def);
    world.DLOG("def : {}", def);
    world.DLOG("id  : {}", def->unique_name());
    world.DLOG("type: {}", def->type());
    world.DLOG("node: {}", def->node_name());
    world.DLOG("ops : {}", def->num_ops());
    world.DLOG("proj: {}", def->num_projs());
    world.DLOG("eops: {}", def->num_extended_ops());
}
} // namespace

template<dbg id>
Ref normalize_dbg(Ref type, Ref callee, Ref arg) {
    auto& world = arg->world();
    debug_print(arg);
    return id == dbg::perm ? world.raw_app(type, callee, arg) : arg;
}

Ref normalize_reify(Ref, Ref, Ref arg) { return do_reify(arg); }

Ref normalize_reflect(Ref, Ref, Ref arg) { return do_reflect(arg); }

Ref normalize_refine(Ref type, Ref callee, Ref arg) {
    auto& world       = arg->world();
    auto [code, i, x] = arg->projs<3>();
    if (auto l = Lit::isa(i)) {
        auto def = do_reflect(code);
        return do_reify(def->refine(*l, do_reflect(x)));
    }

    return world.raw_app(type, callee, arg);
}

Ref normalize_gid(Ref, Ref, Ref arg) { return arg->world().lit_nat(arg->gid()); }

THORIN_refly_NORMALIZER_IMPL

} // namespace thorin::refly
