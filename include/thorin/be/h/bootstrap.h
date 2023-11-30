#pragma once

#include <ostream>

#include "thorin/util/dbg.h"

namespace thorin {

class Driver;

void bootstrap(Driver&, Sym, std::ostream&);

} // namespace thorin
