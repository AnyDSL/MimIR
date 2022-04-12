#ifndef THORIN_BE_LL_LL_H
#define THORIN_BE_LL_LL_H

#include <ostream>

namespace thorin {

class World;

namespace ll {

void emit(World&, std::ostream&);

}
} // namespace thorin

#endif
