#include "mim/phase/phase.h"

namespace mim {

void Phase::run() {
    world().verify().ILOG("=== {}: start ===", name());
    start();
    world().verify().ILOG("=== {}: done ===", name());
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

    for (const auto& [f, def] : world().annexes()) new_world.register_annex(f, rewriter.rewrite(def));
    for (const auto& [_, mut] : world().externals()) {
        auto new_mut = rewriter.rewrite(mut)->as_mut();
        new_mut->make_external();
    }

    swap(world(), new_world);
}

void Pipeline::start() {
    for (auto& phase : phases()) phase->run();
}

} // namespace mim
