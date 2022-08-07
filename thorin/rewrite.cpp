#include "thorin/rewrite.h"

#include "thorin/world.h"

#include "thorin/analyses/scope.h"

namespace thorin {

const Def* Rewriter::rewrite(const Def* old_def) {
    if (auto i = old2new.find(old_def); i != old2new.end()) return i->second;
    if (scope != nullptr && !scope->bound(old_def)) return old_def;

    if (auto [pre, recurse] = pre_rewrite(old_def); pre) {
        auto new_def        = recurse ? rewrite(pre) : pre;
        return old2new[pre] = new_def;
    }

    auto new_type = old_def->type() ? rewrite(old_def->type()) : nullptr;
    auto new_dbg  = old_def->dbg() ? rewrite(old_def->dbg()) : nullptr;

    if (auto infer = old_def->isa_nom<Infer>()) {
        if (auto op = infer->op()) return op;
    }

    if (auto old_nom = old_def->isa_nom()) {
        auto new_nom     = old_nom->stub(new_world, new_type, new_dbg);
        old2new[old_nom] = new_nom;

        for (size_t i = 0, e = old_nom->num_ops(); i != e; ++i) {
            if (auto old_op = old_nom->op(i)) new_nom->set(i, rewrite(old_op));
        }

        if (auto new_def = new_nom->restructure()) return old2new[old_nom] = new_def;

        return new_nom;
    }

    DefArray new_ops(old_def->num_ops(), [&](auto i) { return rewrite(old_def->op(i)); });
    auto new_def = old_def->rebuild(new_world, new_type, new_ops, new_dbg);

    if (auto [post, recurse] = post_rewrite(new_def); post) {
        auto new_def         = recurse ? rewrite(post) : post;
        return old2new[post] = new_def;
    }

    return old2new[old_def] = new_def;
}

const Def* rewrite(const Def* def, const Def* old_def, const Def* new_def, const Scope& scope) {
    Rewriter rewriter(def->world(), &scope);
    rewriter.old2new[old_def] = new_def;
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
    Rewriter rewriter(nom->world(), &scope);
    rewriter.old2new[nom->var()] = arg;
    return DefArray(nom->num_ops(), [&](size_t i) { return rewriter.rewrite(nom->op(i)); });
}

DefArray rewrite(Def* nom, const Def* arg) {
    Scope scope(nom);
    return rewrite(nom, arg, scope);
}

void cleanup(World& old_world) {
    World new_world(old_world.state());
    Rewriter rewriter(old_world, new_world);

    // bring dialects' axioms into new world.
    // TODO making axioms external would render this step superflous
    for (const auto& [_, ax] : old_world.axioms()) rewriter.rewrite(ax);
    for (const auto& [name, nom] : old_world.externals()) rewriter.rewrite(nom)->as_nom()->make_external();

    swap(rewriter.old_world, rewriter.new_world);
}

} // namespace thorin
