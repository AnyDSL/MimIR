#include "mim/plug/compile/pass/prefix_cleanup.h"

#include <iostream>

#include <mim/lam.h>

namespace mim::plug::compile {

void PrefixCleanup::enter() {
    Lam* lam = curr_mut();
    if (lam->sym().view().starts_with(prefix_)) {
        lam->make_internal();
        world().DLOG("internalized {}", lam);
    }
}

} // namespace mim::plug::compile
