#pragma once

#include <ostream>

namespace mim {

class World;

namespace sexpr {

void emit(World&, std::ostream&);

} // namespace sexpr

} // namespace mim
