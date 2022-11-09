#include "thorin/phase/phase.h"

namespace thorin {

void Phase::run() {
    world().ILOG("=== {}: start ===", name());
    start();
    world().ILOG("=== {}: done ===", name());
}

void RWPhase::start() {
    for (const auto& [_, ax] : old_world().axioms()) rewrite(ax);
    for (const auto& [_, nom] : old_world().externals()) rewrite(nom)->as_nom()->make_external();

    swap(Phase::world_, new_world_);
}

void FPPhase::start() {
    for (bool todo = true; todo;) {
        todo = false;
        todo |= analyze();
    }

    RWPhase::start();
}

void Pipeline::start() {
    for (auto& phase : phases()) phase->run();
}

void ScopePhase::start() {
    unique_queue<NomSet> noms;

    for (const auto& [name, nom] : world().externals()) {
        assert(nom->is_set() && "external must not be empty");
        noms.push(nom);
    }

    while (!noms.empty()) {
        auto nom = noms.pop();
        if (elide_empty_ && !nom->is_set()) continue;

        Scope scope(nom);
        scope_ = &scope;
        visit(scope);

        for (auto nom : scope.free_noms()) noms.push(nom);
    }
}

} // namespace thorin
