#pragma once

#include <ostream>

namespace mim {

class World;

namespace plug::spirv {

void emit_asm(World&, std::ostream&);

void emit_bin(World&, std::ostream&);

} // namespace plug::spirv
} // namespace mim
