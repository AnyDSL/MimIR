#include "thorin/fe/ast.h"

#include "thorin/check.h"
#include "thorin/def.h"
#include "thorin/rewrite.h"
#include "thorin/world.h"

#include "thorin/fe/scopes.h"

namespace thorin::fe {

/*
 * bind
 */

void IdPtrn::bind(Scopes& scopes, const Def* def) const { scopes.bind(loc_, sym_, def, rebind()); }

void TuplePtrn::bind(Scopes& scopes, const Def* def) const {
    scopes.bind(loc_, sym_, def, rebind());
    for (size_t i = 0, e = num_ptrns(); i != e; ++i) {
        auto proj = def->proj(e, i)->set(ptrn(i)->loc(), ptrn(i)->sym());
        ptrn(i)->bind(scopes, proj);
    }
}

/*
 * type
 */

const Def* IdPtrn::type(World& world) const {
    if (type_) return type_;
    return type_ = world.nom_infer_type()->set(loc());
}

const Def* TuplePtrn::type(World& world) const {
    if (type_) return type_;

    auto n   = num_ptrns();
    auto ops = Array<const Def*>(n, [&](size_t i) { return ptrn(i)->type(world); });

    if (std::ranges::all_of(ptrns_, [](auto&& b) { return b->is_anonymous(); }))
        return type_ = world.sigma(ops)->set(loc());

    assert(ptrns().size() > 0);

    auto fields = Array<const Def*>(n, [&](size_t i) { return world.tuple_str(ptrn(i)->sym()); });
    auto type   = world.umax<Sort::Type>(ops);
    auto meta   = world.tuple(fields);
    auto sigma  = world.nom_sigma(type, n)->set(loc(), sym(), meta);

    sigma->set(0, ops[0]);
    for (size_t i = 1; i != n; ++i) {
        if (auto infer = infers_[i - 1]) infer->set(sigma->var(n, i - 1)->set(ptrn(i - 1)->sym()));
        sigma->set(i, ops[i]);
    }

    thorin::Scope scope(sigma);
    ScopeRewriter rw(world, scope);
    for (size_t i = 1; i != n; ++i) sigma->set(i, rw.rewrite(ops[i]));

    if (auto restruct = sigma->restructure()) return type_ = restruct;

    return type_ = sigma;
}

} // namespace thorin::fe
