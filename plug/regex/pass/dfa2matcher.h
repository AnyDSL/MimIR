#pragma once
#include "thorin/world.h"

#include "plug/regex/pass/dfa.h"

namespace thorin::regex {
Ref dfa2matcher(World& w, const automaton::DFA& dfa, Ref n);
} // namespace thorin::regex
