#pragma once

#include <ostream>

#include "thorin/util/sym.h"

namespace thorin {

class Driver;

void bootstrap(Driver&, Sym, std::ostream&);

} // namespace thorin
