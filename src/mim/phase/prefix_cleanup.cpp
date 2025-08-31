#include "mim/phase/prefix_cleanup.h"

#include <mim/lam.h>

namespace mim {

PrefixCleanup::PrefixCleanup(World& world, std::string prefix)
    : RWPhase(world, std::format("prefix_cleanup `{}`", prefix))
    , prefix_(std::move(prefix)) {}

void PrefixCleanup::rewrite_externals() {
    for (auto mut : old_world().copy_externals()) {
        if (mut->sym().view().starts_with(prefix_)) {
            mut->make_internal();
            new_world().DLOG("internalized {}", mut);
        } else {
            mut->transfer_external(rewrite(mut)->as_mut());
        }
    }
}

} // namespace mim
