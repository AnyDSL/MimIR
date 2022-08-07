#include "thorin/phase/phase.h"

namespace thorin {

void Phase::run() {
    world().ILOG("=== {}: start ===", name());
    start();
    world().ILOG("=== {}: done ===", name());
}

void RewritePhase::start() {
    for (const auto& [_, ax] : world().axioms()) rewrite(ax);
    for (const auto& [_, nom] : world().externals()) rewrite(nom)->as_nom()->make_external();

    swap(old_world(), new_world());
}

} // namespace thorin
