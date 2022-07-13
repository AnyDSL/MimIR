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

void IdPtrn::bind(Scopes& scopes, const Def* scrutinee) const { scopes.bind(sym_, scrutinee); }

void TuplePtrn::bind(Scopes& scopes, const Def* scrutinee) const {
    scopes.bind(sym_, scrutinee);
    size_t n = ptrns_.size();
    for (size_t i = 0; i != n; ++i) ptrns_[i]->bind(scopes, scrutinee->proj(n, i));
}

/*
 * type
 */

const Def* IdPtrn::type(World& world) const {
    if (type_) return type_;
    return type_ = world.nom_infer_of_infer_level(world.dbg(loc()));
}

const Def* TuplePtrn::type(World& world) const {
    if (type_) return type_;

    auto n   = num_ptrns();
    auto ops = Array<const Def*>(n, [&](size_t i) { return ptrn(i)->type(world); });

    if (std::ranges::all_of(ptrns_, [](auto&& b) { return b->is_anonymous(); }))
        return type_ = world.sigma(ops, world.dbg(loc()));

    assert(ptrns().size() > 0);

    auto fields = Array<const Def*>(n, [&](size_t i) { return ptrn(i)->sym().str(); });
    auto type   = infer_type_level(world, ops);
    auto meta   = world.tuple(fields);
    auto debug  = Debug(sym(), loc(), meta);
    auto sigma  = world.nom_sigma(type, n, world.dbg(debug));

    sigma->set(0, ops[0]);
    for (size_t i = 1; i != n; ++i) {
        if (auto infer = infers_[i - 1]) infer->set(sigma->var(n, i - 1));
        sigma->set(i, ops[i]);
    }

    thorin::Scope scope(sigma);
    Rewriter rw(world, &scope);
    for (size_t i = 1; i != n; ++i) sigma->set(i, rw.rewrite(ops[i]));

    return type_ = sigma;
}

} // namespace thorin::fe
