#include "mim/phase/phase.h"

namespace mim {

void Phase::run() {
    world().verify().ILOG("=== {}: Phase start ===", name());
    start();
    world().verify().ILOG("=== {}: Phase done ===", name());
}

void RWPhase::start() {
    rewrite_annexes();
    bootstrapping_ = false;
    rewrite_externals();

    swap(old_world(), new_world());
}

void RWPhase::rewrite_annexes() {
    for (const auto& [f, def] : old_world().flags2annex())
        new_world().register_annex(f, rewrite(def));
}

void RWPhase::rewrite_externals() {
    for (auto mut : old_world().copy_externals())
        mut->transfer_external(rewrite(mut)->as_mut());
}

void FPPhase::start() {
    for (bool todo = true; todo;) {
        todo = false;
        todo |= analyze();
    }

    RWPhase::start();
}

void Pipeline::start() {
    for (todo_ = true; todo_;) {
        todo_ = false;
        for (auto& phase : phases()) {
            phase->run();
            todo_ |= phase->todo();
        }

        if (todo_)
            for (auto& phase : phases())
                phase->reset();
    }
}

} // namespace mim
