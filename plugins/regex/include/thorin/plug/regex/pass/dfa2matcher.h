#pragma once

#include <automaton/dfa.h>

#include "thorin/plug/regex/regex.h"

namespace thorin::plug::regex {
Ref dfa2matcher(World& w, const automaton::DFA& dfa, Ref n);
} // namespace thorin::plug::regex
