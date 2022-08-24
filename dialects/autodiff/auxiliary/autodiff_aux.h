#pragma once

#include <thorin/def.h>
#include <thorin/lam.h>

namespace thorin::autodiff {

bool is_closed_function(const Def* e);

const Def* id_pullback(const Def*);
const Def* zero_pullback(const Def* E, const Def* A);

const Def* tangent_type_fun(const Def*);
const Def* augment_type_fun(const Def*);
const Pi* autodiff_type_fun(const Def*);
const Pi* pullback_type(const Def*, const Def*);


const Def* zero_def(const Def* T);

} // namespace thorin::autodiff
