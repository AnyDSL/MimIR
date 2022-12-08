#pragma once

#include <ostream>

namespace thorin {

class World;

namespace haskell {

void emit(World&, std::ostream&);

} // namespace haskell
} // namespace thorin
