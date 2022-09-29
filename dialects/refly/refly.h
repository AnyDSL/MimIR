#pragma once

#include <thorin/world.h>

#include "dialects/refly/autogen.h"

/// add c++ bindings to call the axioms here
/// preferably using inlined functions
namespace thorin::refly {

// constructors
inline const Axiom* type_code(World& w) { return w.ax<Code>(); }

void debug_print(const Def* def);
} // namespace thorin::refly
