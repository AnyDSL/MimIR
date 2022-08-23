#pragma once

#include <thorin/def.h>

namespace thorin::autodiff {

bool is_closed_function(const Def* e);

const Def* id_pullback(const Def*);
const Def* zero_pullback(const Def* E, const Def* A);

} // namespace thorin::autodiff
