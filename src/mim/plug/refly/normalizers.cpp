#include <rang.hpp>

#include "mim/world.h"

#include "mim/plug/refly/refly.h"

namespace mim::plug::refly {

static_assert(sizeof(void*) <= sizeof(u64), "pointer doesn't fit into Lit");

namespace {

// The trick is that we simply "box" the pointer of @p def inside a Lit of type `%refly.Code`.
Ref do_reify(const Def* def) {
    auto& world = def->world();
    return world.lit(world.call<Code>(def->type()), reinterpret_cast<u64>(def));
}

// And here we are doing the reverse to retrieve the original pointer again.
const Def* do_reflect(const Def* def) { return reinterpret_cast<const Def*>(def->as<Lit>()->get()); }

void debug_print(Ref lvl, Ref def) {
    auto& world = def->world();
    auto level  = Log::Level::Debug;
    if (auto l = Lit::isa(lvl))
        level = (nat_t)Log::Level::Error <= *l && *l <= (nat_t)Log::Level::Debug ? (Log::Level)*l : Log::Level::Debug;
    world.log().log(level, __FILE__, __LINE__, "{}debug_print: {}{}", rang::fg::yellow, def, rang::fg::reset);
    world.log().log(level, def->loc(), "def : {}", def);
    world.log().log(level, def->loc(), "id  : {}", def->unique_name());
    world.log().log(level, def->type()->loc(), "type: {}", def->type());
    world.log().log(level, def->loc(), "node: {}", def->node_name());
    world.log().log(level, def->loc(), "ops : {}", def->num_ops());
    world.log().log(level, def->loc(), "proj: {}", def->num_projs());
    world.log().log(level, def->loc(), "eops: {}", def->num_deps());
}

} // namespace

template<dbg id> Ref normalize_dbg(Ref, Ref, Ref arg) {
    auto [lvl, x] = arg->projs<2>();
    debug_print(lvl, x);
    return id == dbg::perm ? Ref() : Ref(x);
}

Ref normalize_reify(Ref, Ref, Ref arg) { return do_reify(arg); }

Ref normalize_reflect(Ref, Ref, Ref arg) { return do_reflect(arg); }

Ref normalize_refine(Ref, Ref, Ref arg) {
    auto [code, i, x] = arg->projs<3>();
    if (auto l = Lit::isa(i)) {
        auto def = do_reflect(code);
        return do_reify(def->refine(*l, do_reflect(x)));
    }

    return {};
}

Ref normalize_gid(Ref, Ref, Ref arg) { return arg->world().lit_nat(arg->gid()); }

MIM_refly_NORMALIZER_IMPL

} // namespace mim::plug::refly
