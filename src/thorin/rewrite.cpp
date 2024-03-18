#include "thorin/rewrite.h"

#include "thorin/world.h"

#include "thorin/analyses/scope.h"

// Don't use fancy C++-lambdas; it's way to annoying stepping through them in a debugger.

namespace thorin {

Ref Rewriter::rewrite(Ref old_def) {
    if (old_def->isa<Univ>()) return world().univ();
    if (auto i = old2new_.find(old_def); i != old2new_.end()) return i->second;
    if (auto old_mut = old_def->isa_mut()) return rewrite_mut(old_mut);
    return map(old_def, rewrite_imm(old_def));
}

Ref Rewriter::rewrite_imm(Ref old_def) {
    // Extracts are used as conditional branches: make sure that we don't rewrite unreachable stuff.
    if (auto extract = old_def->isa<Extract>()) {
        if (auto index = Lit::isa(rewrite(extract->index()))) {
            if (auto tuple = extract->tuple()->isa<Tuple>()) return rewrite(tuple->op(*index));
            if (auto pack = extract->tuple()->isa_imm<Pack>(); pack && pack->shape()->dep_const())
                return rewrite(pack->body());
        }
    }

    auto new_type = old_def->isa<Type>() ? nullptr : rewrite(old_def->type());
    auto new_ops  = DefVec(old_def->num_ops());
    for (size_t i = 0, e = new_ops.size(); i != e; ++i) new_ops[i] = rewrite(old_def->op(i));
    return old_def->rebuild(world(), new_type, new_ops);
}

Ref Rewriter::rewrite_mut(Def* old_mut) {
    auto new_type = rewrite(old_mut->type());
    auto new_mut  = old_mut->stub(world(), new_type);
    map(old_mut, new_mut);

    if (old_mut->is_set()) {
        for (size_t i = 0, e = old_mut->num_ops(); i != e; ++i) new_mut->set(i, rewrite(old_mut->op(i)));
        if (auto new_imm = new_mut->immutabilize()) return map(old_mut, new_imm);
    }

    return new_mut;
}

Ref rewrite(Ref def, Ref old_def, Ref new_def, const Scope& scope) {
    ScopeRewriter rewriter(scope);
    rewriter.map(old_def, new_def);
    return rewriter.rewrite(def);
}

Ref rewrite(Def* mut, Ref arg, size_t i, const Scope& scope) { return rewrite(mut->op(i), mut->var(), arg, scope); }

Ref rewrite(Def* mut, Ref arg, size_t i) {
    Scope scope(mut);
    return rewrite(mut, arg, i, scope);
}

DefVec rewrite(Def* mut, Ref arg, const Scope& scope) {
    ScopeRewriter rewriter(scope);
    rewriter.map(mut->var(), arg);
    DefVec result(mut->num_ops());
    for (size_t i = 0, e = result.size(); i != e; ++i) result[i] = rewriter.rewrite(mut->op(i));
    return result;
}

DefVec rewrite(Def* mut, Ref arg) {
    if (auto var = mut->has_var()) {
        auto rw = VarRewriter(var, arg);
        // outln("reduce: {} -- {} -- {}", mut, var, arg);
        DefVec result(mut->num_ops());
        for (size_t i = 0, e = result.size(); i != e; ++i) result[i] = rw.rewrite(mut->op(i));
        return result;
    }
    return DefVec(mut->ops().begin(), mut->ops().end());
}

} // namespace thorin
