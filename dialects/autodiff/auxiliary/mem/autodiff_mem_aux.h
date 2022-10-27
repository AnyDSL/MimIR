#pragma once

#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/autodiff/passes/autodiff_eval.h"

namespace thorin::autodiff {
const Def* shadow_array_type(const Def* def, const Def* arg_ty);

// TODO: names not self explanatory
Lam* mem_return_lam(World& w, std::string name, Defs domain = {}, bool filter = true);
Lam* return_lam(World& w, std::string name, Defs domain = {}, bool filter = true);
Lam* mem_lam(World& w, std::string name, bool filter = true);

// TODO: names not self explanatory
Lam* call_pullback_ptr(const Def* gradient_ptr, const Def* pullback_ptr);
Lam* call_pullback_arr(const Def* gradient_arr, const Def* pullback_arr);
Lam* call_pullbacks(const Def* gradient, const Def* pullback);

const Lam* wrap_append_app(const Def* lam, const Def* call_after_lam);

} // namespace thorin::autodiff
