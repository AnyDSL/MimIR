#pragma once
#include "thorin/world.h"

#include "thorin/plug/regex/pass/dfa.h"

namespace thorin::plug::regex {
Ref dfa2matcher(World& w, const automaton::DFA& dfa, Ref n);
} // namespace thorin::plug::regex
