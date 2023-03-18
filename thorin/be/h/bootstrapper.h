#pragma once

#include <deque>
#include <ostream>
#include <string>
#include <string_view>

#include "thorin/util/print.h"
#include "thorin/util/sym.h"
#include "thorin/util/types.h"

namespace thorin {

class Driver;

void bootstrap(Driver&, Sym, std::ostream&);

} // namespace thorin::h
