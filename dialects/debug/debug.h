#pragma once

#include <thorin/world.h>

#include "dialects/debug/autogen.h"

/// add c++ bindings to call the axioms here
/// preferably using inlined functions
namespace thorin::debug {
void debug_print(const Def* def);
void debug_print(const char* head, const Def* def);
} // namespace thorin::debug
