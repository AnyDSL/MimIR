#include "mim/subst.h"

#include <absl/container/fixed_array.h>

#include "mim/world.h"

// Don't use fancy C++-lambdas; it's way too annoying stepping through them in a debugger.

namespace mim {

const Def* Subst::subst(const Def* old_def) {
    if (old_def->isa<Univ>()) return world().univ();
    if (auto i = old2new_.find(old_def); i != old2new_.end()) return i->second;
    if (auto old_mut = old_def->isa_mut()) return subst_mut(old_mut);
    return map(old_def, subst_imm(old_def));
}

const Def* Subst::subst_imm(const Def* old_def) {
    // Extracts are used as conditional branches: make sure that we don't subst unreachable stuff.
    if (auto extract = old_def->isa<Extract>()) {
        if (auto index = Lit::isa(subst(extract->index()))) {
            if (auto tuple = extract->tuple()->isa<Tuple>()) return subst(tuple->op(*index));
            if (auto pack = extract->tuple()->isa_imm<Pack>(); pack && pack->shape()->is_closed())
                return subst(pack->body());
        }
    }

    auto new_type = old_def->isa<Type>() ? nullptr : subst(old_def->type());
    auto size     = old_def->num_ops();
    auto new_ops  = absl::FixedArray<const Def*>(size);
    for (size_t i = 0; i != size; ++i) new_ops[i] = subst(old_def->op(i));
    return old_def->rebuild(world(), new_type, new_ops);
}

const Def* Subst::subst_mut(Def* old_mut) {
    auto new_type = subst(old_mut->type());
    auto new_mut  = old_mut->stub(world(), new_type);
    map(old_mut, new_mut);

    if (old_mut->is_set()) {
        for (size_t i = 0, e = old_mut->num_ops(); i != e; ++i) new_mut->set(i, subst(old_mut->op(i)));
        if (auto new_imm = new_mut->immutabilize()) return map(old_mut, new_imm);
    }

    return new_mut;
}

} // namespace mim
