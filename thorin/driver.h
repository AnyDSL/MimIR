#pragma once

#include "thorin/flags.h"
#include "thorin/world.h"

#include "thorin/util/log.h"

namespace thorin {

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
