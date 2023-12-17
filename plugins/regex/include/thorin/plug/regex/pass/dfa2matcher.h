#pragma once

#include <thorin/world.h>

#include "automaton/dfa.h"

namespace thorin::plug::regex {
THORIN_EXPORT Ref dfa2matcher(World& w, const automaton::DFA& dfa, Ref n);
} // namespace thorin::plug::regex
