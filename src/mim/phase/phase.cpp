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

Pipeline::Pipeline(World& world, bool fixed_point)
    : Phase(world, std::format("pipeline (fixed_point_: `{}`)", fixed_point))
    , fixed_point_(fixed_point) {}

void Pipeline::start() {
    do {
        for (auto& phase : phases()) {
            phase->run();
            todo_ |= phase->todo();
        }

        todo_ &= fixed_point();

        if (todo_) {
            for (auto& phase : phases())
                phase->reset();
            reset();
        }
    } while (todo_);
}

} // namespace mim
