#pragma once

#include "thorin/flags.h"
#include "thorin/world.h"

#include "thorin/util/log.h"

namespace thorin {

/// Some "global" variables needed all over the place.
/// Well, there are not really global - that's the point of this class.
class Driver : public SymPool {
public:
    Driver()
        : log(*this)
        , world(this) {}

    Flags flags;
    Log log;
#if THORIN_ENABLE_CHECKS
    absl::flat_hash_set<uint32_t> breakpoints;
#endif
    World world;
};

} // namespace thorin
