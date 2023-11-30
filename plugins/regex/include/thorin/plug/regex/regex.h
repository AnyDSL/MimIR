#pragma once

#include "thorin/plug/regex/autogen.h"

namespace thorin {
template<> struct Axiom::Match<plug::regex::any> {
    using type = Axiom;
};
} // namespace thorin
