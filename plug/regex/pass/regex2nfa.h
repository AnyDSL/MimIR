#pragma once
#include "plug/regex/pass/nfa.h"
#include "plug/regex/regex.h"

namespace thorin {
namespace regex {
std::unique_ptr<automaton::NFA> regex2nfa(Ref regex);
}
} // namespace thorin
