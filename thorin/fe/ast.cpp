#include "thorin/fe/ast.h"

#include "thorin/check.h"
#include "thorin/def.h"
#include "thorin/rewrite.h"
#include "thorin/world.h"

#include "thorin/fe/scopes.h"

namespace thorin::fe {

const Def* Ptrn::dbg(World& world) { return world.dbg(Debug(sym(), loc())); }

/*
 * bind
 */

void IdPtrn::bind(Scopes& scopes, const Def* def) const { scopes.bind(sym_, def); }

void TuplePtrn::bind(Scopes& scopes, const Def* def) const {
    World& w = def->world();
    scopes.bind(sym_, def);
    for (size_t i = 0, e = num_ptrns(); i != e; ++i) ptrn(i)->bind(scopes, def->proj(e, i, w.dbg(ptrn(i)->sym())));
}

/*
 * type
 */

const Def* IdPtrn::type(World& world) const {
    if (type_) return type_;
    return type_ = world.nom_infer_type(world.dbg(loc()));
}

const Def* TuplePtrn::type(World& world) const {
    if (type_) return type_;

    auto n   = num_ptrns();
    auto ops = Array<const Def*>(n, [&](size_t i) { return ptrn(i)->type(world); });

    if (std::ranges::all_of(ptrns_, [](auto&& b) { return b->is_anonymous(); }))
        return type_ = world.sigma(ops, world.dbg(loc()));

    assert(ptrns().size() > 0);

    auto fields = Array<const Def*>(n, [&](size_t i) { return ptrn(i)->sym().str(); });
    auto type   = world.umax<Sort::Type>(ops);
    auto meta   = world.tuple(fields);
    auto debug  = Debug(sym(), loc(), meta);
    auto sigma  = world.nom_sigma(type, n, world.dbg(debug));

    sigma->set(0, ops[0]);
    for (size_t i = 1; i != n; ++i) {
        if (auto infer = infers_[i - 1]) infer->set(sigma->var(n, i - 1, world.dbg(ptrn(i - 1)->sym())));
        sigma->set(i, ops[i]);
    }

    thorin::Scope scope(sigma);
    ScopeRewriter rw(world, scope);
    for (size_t i = 1; i != n; ++i) sigma->set(i, rw.rewrite(ops[i]));

    if (auto restruct = sigma->restructure()) return type_ = restruct;

    return type_ = sigma;
}

} // namespace thorin::fe
