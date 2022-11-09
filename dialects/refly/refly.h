#pragma once

#include <thorin/world.h>

#include "dialects/refly/autogen.h"

/// add c++ bindings to call the axioms here
/// preferably using inlined functions
namespace thorin::refly {

// constructors
inline const Axiom* type_def(thorin::World& w) { return w.ax<Def>(); }
inline const Axiom* type_world(thorin::World& w) { return w.ax<World>(); }

void debug_print(const thorin::Def*);
} // namespace thorin::refly
