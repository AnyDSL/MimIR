#pragma once

#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

namespace thorin::autodiff {
const Def* create_ho_pb_type(const Def* def, const Def* arg_ty);
} // namespace thorin::autodiff
