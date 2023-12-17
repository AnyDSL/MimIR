#pragma once

#include <memory>

#include "automaton/dfa.h"

namespace automaton {
std::unique_ptr<DFA> minimize_dfa(const DFA& dfa);
} // namespace automaton
