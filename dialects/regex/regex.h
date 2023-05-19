#pragma once

#include "dialects/regex/autogen.h"
namespace thorin {
template<>
struct Axiom::Match<regex::cls> {
    using type = Axiom;
};
} // namespace thorin
