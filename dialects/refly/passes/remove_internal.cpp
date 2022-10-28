#include "dialects/refly/passes/remove_internal.h"

#include <iostream>

#include <thorin/lam.h>

namespace thorin::refly {

void InternalCleanup::enter() {
    Lam* lam = curr_nom();
    if (lam->name().starts_with("internal_")) {
        lam->make_internal();
        world().DLOG("internalized {}", lam);
    }
}

} // namespace thorin::refly
