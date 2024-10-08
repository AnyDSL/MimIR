#pragma once

#include <automaton/nfa.h>

#include "mim/plug/regex/regex.h"

/// You can dl::get this function.
/// @returns a raw pointer to automanton::NFA; use mim::regex::regex2nfa to pack it into a `std::unique_ptr`.
extern "C" THORIN_EXPORT automaton::NFA* regex2nfa(mim::Ref regex);

namespace mim::plug::regex {

std::unique_ptr<automaton::NFA> regex2nfa(Ref regex);

inline std::unique_ptr<automaton::NFA> regex2nfa(decltype(::regex2nfa)* fptr, Ref regex) {
    return std::unique_ptr<automaton::NFA>(fptr(regex), std::default_delete<automaton::NFA>());
}

} // namespace mim::plug::regex
