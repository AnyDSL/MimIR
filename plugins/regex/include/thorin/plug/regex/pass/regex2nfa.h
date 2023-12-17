#pragma once

#include "thorin/plug/regex/automaton/nfa.h"
#include "thorin/plug/regex/regex.h"

namespace thorin::plug::regex {
std::unique_ptr<automaton::NFA> regex2nfa(Ref regex);
} // namespace thorin::plug::regex
