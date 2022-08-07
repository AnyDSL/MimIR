#include "thorin/phase/phase.h"

namespace thorin {

std::pair<const Def*, bool> PhaseRewriter::pre_rewrite(const Def* def) { return phase.pre_rewrite(def); }
std::pair<const Def*, bool> PhaseRewriter::post_rewrite(const Def* def) { return phase.post_rewrite(def); }

void Phase::run() {
    world().ILOG("=== {}: start ===", name());
    start();
    world().ILOG("=== {}: done ===", name());
}

void RWPhase::start() {
    for (const auto& [_, ax] : world().axioms()) rewrite(ax);
    for (const auto& [_, nom] : world().externals()) rewrite(nom)->as_nom()->make_external();

    swap(old_world(), new_world());
}

void FPPhase::start() {
    for (bool todo = true; todo;) {
        todo = false;
        todo |= analyze();
    }

    RWPhase::start();
}

} // namespace thorin
