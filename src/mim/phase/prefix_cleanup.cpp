#include "mim/phase/prefix_cleanup.h"

#include <mim/lam.h>

namespace mim {

void PrefixCleanup::start() {
    for (auto mut : old_world().copy_externals()) {
        if (mut->sym().view().starts_with(prefix_)) {
            mut->make_internal();
            new_world().DLOG("internalized {}", mut);
        }
    }

    RWPhase::start();
}

} // namespace mim
