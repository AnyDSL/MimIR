#pragma once

#include <automaton/dfa.h>

#include "thorin/plug/regex/regex.h"

/// You can dl::get this function.
extern "C" THORIN_EXPORT const thorin::Def* dfa2matcher(thorin::World&, const automaton::DFA&, thorin::Ref);
