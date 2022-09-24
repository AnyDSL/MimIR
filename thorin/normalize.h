#pragma once

#include "thorin/def.h"
#include "thorin/world.h"

namespace thorin {

const Def* normalize_zip(const Def*, const Def*, const Def*, const Def*);

template<Trait>
const Def* normalize_Trait(const Def*, const Def*, const Def*, const Def*);
template<PE>
const Def* normalize_PE(const Def*, const Def*, const Def*, const Def*);

} // namespace thorin
