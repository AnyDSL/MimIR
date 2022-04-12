#ifndef THORIN_BE_DOT_DOT_H
#define THORIN_BE_DOT_DOT_H

#include <ostream>
#include <functional>

namespace thorin {
class World;
class Def;

namespace dot {
void default_stream_def(std::ostream&, const Def*);
void emit(World& w, std::ostream& s, std::function<void(std::ostream&, const Def*)> stream_def = default_stream_def);

} // namespace dot
} // namespace thorin

#endif
