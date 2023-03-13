#include "dialects/compile/passes/internal_cleanup.h"

#include <iostream>

#include <thorin/lam.h>

namespace thorin::compile {

void InternalCleanup::enter() {
    Lam* lam = curr_nom();
    if (lam->sym()->starts_with(prefix_)) {
        lam->make_internal();
        world().DLOG("internalized {}", lam);
    }
}

} // namespace thorin::compile
