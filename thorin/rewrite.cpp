#include "thorin/rewrite.h"

#include "thorin/world.h"

#include "thorin/analyses/scope.h"

namespace thorin {

const Def* Rewriter::rewrite(Ref old_def) {
    if (!old_def) return nullptr;
    if (old_def->isa<Univ>()) return world().univ();
    if (auto i = old2new_.find(old_def); i != old2new_.end()) return i->second;
    if (auto old_nom = old_def->isa_nom()) return rewrite_nom(old_nom);

    auto new_def = rewrite_structural(old_def);
    return map(old_def, new_def);
}

const Def* Rewriter::rewrite_structural(const Def* old_def) {
    auto new_type = rewrite(old_def->type());
    auto new_dbg  = rewrite(old_def->dbg());
    DefArray new_ops(old_def->num_ops(), [&](auto i) { return rewrite(old_def->op(i)); });
    return old_def->rebuild(world(), new_type, new_ops, new_dbg);
}

const Def* Rewriter::rewrite_nom(Def* old_nom) {
    auto new_type = rewrite(old_nom->type());
    auto new_dbg  = rewrite(old_nom->dbg());
    auto new_nom  = old_nom->stub(world(), new_type, new_dbg);
    map(old_nom, new_nom);

    if (old_nom->is_set()) {
        for (size_t i = 0, e = old_nom->num_ops(); i != e; ++i) new_nom->set(i, rewrite(old_nom->op(i)));
        if (auto new_def = new_nom->restructure()) return map(old_nom, new_def);
    }

    return new_nom;
}

const Def* rewrite(const Def* def, const Def* old_def, const Def* new_def, const Scope& scope) {
    ScopeRewriter rewriter(def->world(), scope);
    rewriter.map(old_def, new_def);
    return rewriter.rewrite(def);
}

const Def* rewrite(Def* nom, const Def* arg, size_t i, const Scope& scope) {
    return rewrite(nom->op(i), nom->var(), arg, scope);
}

const Def* rewrite(Def* nom, const Def* arg, size_t i) {
    Scope scope(nom);
    return rewrite(nom, arg, i, scope);
}

DefArray rewrite(Def* nom, const Def* arg, const Scope& scope) {
    ScopeRewriter rewriter(nom->world(), scope);
    rewriter.map(nom->var(), arg);
    return DefArray(nom->num_ops(), [&](size_t i) { return rewriter.rewrite(nom->op(i)); });
}

DefArray rewrite(Def* nom, const Def* arg) {
    Scope scope(nom);
    return rewrite(nom, arg, scope);
}

} // namespace thorin
