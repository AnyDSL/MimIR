#include "thorin/plug/compile/pass/internal_cleanup.h"

#include <iostream>

#include <thorin/lam.h>

namespace thorin::plug::compile {

void InternalCleanup::enter() {
    Lam* lam = curr_mut();
    if (lam->sym().view().starts_with(prefix_)) {
        lam->make_internal();
        world().DLOG("internalized {}", lam);
    }
}

} // namespace thorin::plug::compile
