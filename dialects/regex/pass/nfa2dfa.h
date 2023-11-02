#pragma once
#include <memory>

#include "dialects/regex/pass/dfa.h"
#include "dialects/regex/pass/nfa.h"

namespace thorin {
namespace automaton {
std::unique_ptr<DFA> nfa2dfa(const NFA& nfa);
} // namespace automaton
} // namespace thorin
