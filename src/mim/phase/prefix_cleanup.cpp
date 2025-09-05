#include "mim/phase/prefix_cleanup.h"

#include <mim/lam.h>

namespace mim {

void PrefixCleanup::set(const App* app) { prefix_ = tuple2str(app->arg()); }

void PrefixCleanup::rewrite_external(Def* mut) {
    if (mut->sym().view().starts_with(prefix_)) {
        mut->make_internal();
        new_world().DLOG("internalized {}", mut);
    } else {
        mut->transfer_external(rewrite(mut)->as_mut());
    }
}

} // namespace mim
