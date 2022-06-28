#pragma once

#include <functional>
#include <ostream>

namespace thorin {
class World;
class Def;

namespace dot {
void default_stream_def(std::ostream&, const Def*);
void emit(World& w, std::ostream& s, std::function<void(std::ostream&, const Def*)> stream_def = default_stream_def);

} // namespace dot
} // namespace thorin
