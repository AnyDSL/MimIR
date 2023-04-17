#pragma once

#include <thorin/world.h>

#include "dialects/refly/autogen.h"

namespace thorin::refly {

/// @name %%refly.Code
///@{
inline const Axiom* type_code(World& w) { return w.ax<Code>(); }
///@}

} // namespace thorin::refly
