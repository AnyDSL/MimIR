#pragma once

#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>
#include <thorin/tuple.h>

namespace thorin::autodiff {

const Def* is_idx(const Def* ty);
const Def* zero(const Def* ty);
const Def* one(const Def* ty);
const Def* zero(World& w);
const Def* one(World& w);

const Def* default_def(const Def* ty);
const Def* one_hot_other_default(const Def* pattern, const Def* def, size_t position);

const Def* tangent_type_fun(const Def*);
// R const Def* augment_type_fun(const Def*);
const Pi* autodiff_type_fun_pi(const Pi*, bool flat = true);
const Def* autodiff_type_fun(const Def*, bool flat = true);
const Def* flatten_deep(const Def* def);

const Def* mask(const Def* target, size_t i, const Def* def);
const Def* mask_last(const Def* target, const Def* def);
const Def* merge_flat(const Def* left, const Def* right);

const Extract* is_branch(const Def* callee);
const Var* is_var(const Def* def);
const App* is_load_val(const Def* def);
void for_each_lam(const Def* def, std::function<void(Lam*)> f);
const Def* unextract(const Def* mem);
const Def* arg(const App* app);
} // namespace thorin::autodiff

namespace thorin {

bool is_continuation_type(const Def* E);

} // namespace thorin
