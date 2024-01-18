#pragma once

#include <memory>

#include "automaton/dfa.h"
#include "automaton/nfa.h"

namespace automaton {
std::unique_ptr<DFA> nfa2dfa(const NFA& nfa);
} // namespace automaton
