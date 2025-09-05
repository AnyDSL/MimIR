#include "mim/phase/prefix_cleanup.h"

#include <mim/lam.h>

namespace mim {

void PrefixCleanup::apply(std::string prefix) {
    prefix_ = prefix;
    name_ += " \"" + prefix_ + " \"";
}

void PrefixCleanup::apply(const App* app) { apply(tuple2str(app->arg())); }
void PrefixCleanup::apply(Phase& phase) { apply(std::move(static_cast<PrefixCleanup&>(phase).prefix_)); }

void PrefixCleanup::rewrite_external(Def* mut) {
    if (mut->sym().view().starts_with(prefix_)) {
        mut->make_internal();
        new_world().DLOG("internalized {}", mut);
    } else {
        mut->transfer_external(rewrite(mut)->as_mut());
    }
}

} // namespace mim
