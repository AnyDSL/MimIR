#include "mim/phase/prefix_cleanup.h"

#include <mim/lam.h>

namespace mim {

void PrefixCleanup::apply(std::string prefix) {
    prefix_ = prefix;
    name_ += " \"" + prefix_ + " \"";
}

void PrefixCleanup::apply(const App* app) { apply(tuple2str(app->arg())); }
void PrefixCleanup::apply(Phase& phase) { apply(std::move(static_cast<PrefixCleanup&>(phase).prefix_)); }

void PrefixCleanup::rewrite_external(Def* old_mut) {
    auto new_mut = rewrite(old_mut)->as_mut();
    if (old_mut->is_external()) {
        if (old_mut->sym().view().starts_with(prefix_))
            DLOG("internalized: `{}`", old_mut);
        else
            new_mut->make_external();
    }
}

} // namespace mim
