#pragma once

#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

namespace thorin::autodiff {

const Def* tangent_type_fun(const Def*);
// R const Def* augment_type_fun(const Def*);
const Pi* autodiff_type_fun_pi(const Pi*, bool flat = true);
const Def* autodiff_type_fun(const Def*, bool flat = true);
const Pi* pullback_type(const Def* in, const Def* out, bool flat = true);
const Pi* forward_to_backward(const Pi* forward_ty);
const Def* flatten_deep(const Def* def);
Lam* callee_isa_var(const Def* def);

const Def* mask(const Def* target, size_t i, const Def* def);
const Def* mask_last(const Def* target, const Def* def);
const Def* merge_flat(const Def* left, const Def* right);

} // namespace thorin::autodiff

namespace thorin {

bool is_continuation_type(const Def* E);

const Def* continuation_dom(const Def* E);

} // namespace thorin
