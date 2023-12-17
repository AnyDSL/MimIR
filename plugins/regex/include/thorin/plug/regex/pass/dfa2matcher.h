#pragma once

#include <thorin/world.h>

#include "thorin/plug/regex/automaton/dfa.h"

namespace thorin::plug::regex {
Ref dfa2matcher(World& w, const automaton::DFA& dfa, Ref n);
} // namespace thorin::plug::regex
