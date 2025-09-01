#include "mim/phase/phase.h"

namespace mim {

void Phase::run() {
    world().verify().ILOG("=== {}: Phase start ===", name());
    start();
    world().verify().ILOG("=== {}: Phase done ===", name());
}

void RWPhase::start() {
    for (const auto& [f, def] : old_world().flags2annex())
        rewrite_annex(f, def);

    bootstrapping_ = false;

    for (auto mut : old_world().copy_externals())
        rewrite_external(mut);

    swap(old_world(), new_world());
}

void RWPhase::rewrite_annex(flags_t f, const Def* def) { new_world().register_annex(f, rewrite(def)); }

void RWPhase::rewrite_external(Def* mut) { mut->transfer_external(rewrite(mut)->as_mut()); }

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
