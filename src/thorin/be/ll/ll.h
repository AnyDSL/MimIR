#ifndef THORIN_BE_LL_LL_H
#define THORIN_BE_LL_LL_H

#include "thorin/config.h"

namespace thorin {

class Stream;
class World;

namespace ll {

THORIN_API void emit(World&, Stream&);

}
} // namespace thorin

#endif
