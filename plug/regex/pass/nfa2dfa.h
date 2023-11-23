#pragma once
#include <memory>

#include "plug/regex/pass/dfa.h"
#include "plug/regex/pass/nfa.h"

namespace thorin {
namespace automaton {
std::unique_ptr<DFA> nfa2dfa(const NFA& nfa);
} // namespace automaton
} // namespace thorin
