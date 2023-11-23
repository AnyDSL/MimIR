#pragma once

#include "plug/regex/autogen.h"

namespace thorin {
template<> struct Axiom::Match<regex::any> {
    using type = Axiom;
};
} // namespace thorin
