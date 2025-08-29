#include "mim/phase/phase.h"

namespace mim {

void Phase::run() {
    world().verify().ILOG("=== {}: start ===", name());
    start();
    world().verify().ILOG("=== {}: done ===", name());
}

void RWPhase::start() {
    for (auto def : world().annexes())
        rewrite(def);
    for (auto mut : world().copy_externals())
        mut->transfer_external(rewrite(mut)->as_mut());
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

    for (const auto& [f, def] : world().flags2annex())
        new_world.register_annex(f, rewriter.rewrite(def));
    for (auto mut : world().externals()) {
        auto new_mut = rewriter.rewrite(mut)->as_mut();
        new_mut->make_external();
    }

    swap(world(), new_world);
}

void Pipeline::start() {
    for (todo_ = true; todo_;) {
        todo_ = false;
        for (auto& phase : phases()) {
            phase->run();
            // todo_ |= phase->todo();
        }
    }
}

} // namespace mim
