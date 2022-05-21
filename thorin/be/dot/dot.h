#ifndef THORIN_BE_DOT_DOT_H
#define THORIN_BE_DOT_DOT_H

#include <functional>

namespace thorin {
class World;
class Stream;
class Def;

namespace dot {
void default_stream_def(Stream&, const Def*);
void emit(World& w, Stream& s, std::function<void(Stream&, const Def*)> stream_def = default_stream_def);

} // namespace dot
} // namespace thorin

#endif
