#include "thorin/phase/phase.h"

#include <vector>

namespace thorin {

void Phase::run() {
    world().ILOG("=== {}: start ===", name());
    start();
    world().ILOG("=== {}: done ===", name());
}

void RWPhase::start() {
    for (const auto& [_, def] : world().annexes()) rewrite(def);
    auto externals = world().externals();
    for (const auto& [_, mut] : externals) mut->transfer_external(rewrite(mut)->as_mut());
}

void FPPhase::start() {
    for (bool todo = true; todo;) {
        todo = false;
        todo |= analyze();
    }

    RWPhase::start();
}

void Cleanup::start() {
    auto new_world = world().inherit();
    Rewriter rewriter(new_world);

    for (const auto& [_, def] : world().annexes()) rewriter.rewrite(def);
    for (const auto& [_, mut] : world().externals()) rewriter.rewrite(mut)->as_mut()->make_external(true);

    swap(world(), new_world);
}

void Pipeline::start() {
    for (auto& phase : phases()) phase->run();
}

void ScopePhase::start() {
    unique_queue<MutSet> muts;

    for (const auto& [name, mut] : world().externals()) {
        assert(mut->is_set() && "external must not be empty");
        muts.push(mut);
    }

    while (!muts.empty()) {
        auto mut = muts.pop();
        if (elide_empty_ && !mut->is_set()) continue;

        Scope scope(mut);
        scope_ = &scope;
        visit(scope);

        for (auto mut : scope.free_muts()) muts.push(mut);
    }
}

} // namespace thorin
