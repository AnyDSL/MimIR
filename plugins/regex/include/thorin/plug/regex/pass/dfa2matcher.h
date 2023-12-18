#pragma once

#include <automaton/dfa.h>

#include "thorin/plug/regex/regex.h"

namespace thorin::plug::regex {
THORIN_regex_API Ref dfa2matcher(World& w, const automaton::DFA& dfa, Ref n);
} // namespace thorin::plug::regex
