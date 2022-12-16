#include "thorin/phase/phase.h"

#include <vector>

namespace thorin {

void Phase::run() {
    world().ILOG("=== {}: start ===", name());
    start();
    world().ILOG("=== {}: done ===", name());
}

void RWPhase::start() {
    for (const auto& [_, ax] : world().axioms()) rewrite(ax);
    std::vector<Def*> new_internals;
    std::vector<Def*> new_externals;
    for (const auto& [_, nom] : world().externals()) {
        auto rewritten = rewrite(nom);
        new_externals.push_back(rewritten->as_nom());
        new_internals.push_back(nom);
    }
    for (auto nom : new_internals) world().make_internal(nom);
    for (auto nom : new_externals) world().make_external(nom);
    // world().debug_dump();
    // assert(0);
}

void FPPhase::start() {
    for (bool todo = true; todo;) {
        todo = false;
        todo |= analyze();
    }

    RWPhase::start();
}

void Cleanup::start() {
    World new_world(world().state());
    Rewriter rewriter(new_world);

    for (const auto& [_, ax] : world().axioms()) rewriter.rewrite(ax);
    for (const auto& [_, nom] : world().externals()) rewriter.rewrite(nom)->as_nom()->make_external();

    swap(world(), new_world);
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
