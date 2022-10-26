#pragma once

#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/autodiff/passes/autodiff_eval.h"

namespace thorin::autodiff {
const Def* shadow_array_type(const Def* def, const Def* arg_ty);

} // namespace thorin::autodiff
