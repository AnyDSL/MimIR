#pragma once

#include <memory>

#include "dfa.h"

namespace thorin::automaton {
std::unique_ptr<DFA> minimize_dfa(const DFA& dfa);
} // namespace thorin::automaton
