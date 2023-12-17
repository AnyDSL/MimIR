#pragma once
#include <memory>

#include "thorin/plug/regex/automaton/dfa.h"
#include "thorin/plug/regex/automaton/nfa.h"

namespace thorin {
namespace automaton {
std::unique_ptr<DFA> nfa2dfa(const NFA& nfa);
} // namespace automaton
} // namespace thorin
