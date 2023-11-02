#pragma once
#include "dialects/regex/pass/nfa.h"
#include "dialects/regex/regex.h"

namespace thorin {
namespace regex {
std::unique_ptr<automaton::NFA> regex2nfa(Ref regex);
}
} // namespace thorin
