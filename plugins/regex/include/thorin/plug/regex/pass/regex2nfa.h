#pragma once

#include <automaton/nfa.h>

#include "thorin/plug/regex/regex.h"

namespace thorin::plug::regex {
THORIN_regex_API std::unique_ptr<automaton::NFA> regex2nfa(Ref regex);
} // namespace thorin::plug::regex
