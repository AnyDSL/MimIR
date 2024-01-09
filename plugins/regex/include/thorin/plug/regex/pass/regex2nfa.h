#pragma once

#include <automaton/nfa.h>

#include "thorin/plug/regex/regex.h"

extern "C" std::unique_ptr<automaton::NFA> THORIN_EXPORT regex2nfa(thorin::Ref regex);
