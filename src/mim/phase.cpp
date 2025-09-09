#include "mim/phase.h"

#include <memory>

#include "mim/driver.h"

namespace mim {

/*
 * Phase
 */

void Phase::run() {
    world().verify().ILOG("=== 🚀 Phase launch: `{}` ===", name());
    start();
    world().verify().ILOG("=== 🏁 Phase finish: `{}` ===", name());
}

/*
 * RWPhase
 */

void RWPhase::start() {
    set(old_world().inherit());

    for (const auto& [f, def] : old_world().flags2annex())
        rewrite_annex(f, def);

    bootstrapping_ = false;

    for (auto mut : old_world().copy_externals())
        rewrite_external(mut);

    assert(old_world().incarnation() + 1 == new_world().incarnation());
    swap(old_world(), new_world());
}

void RWPhase::rewrite_annex(flags_t f, const Def* def) { new_world().register_annex(f, rewrite(def)); }

void RWPhase::rewrite_external(Def* old_mut) {
    auto new_mut = rewrite(old_mut)->as_mut();
    if (old_mut->is_external()) new_mut->make_external();
}

/*
 * FPPhase
 */

void FPPhase::start() {
    for (bool todo = true; todo;) {
        todo = false;
        todo |= analyze();
    }

    RWPhase::start();
}

/*
 * PhaseMan
 */

PhaseMan::PhaseMan(World& world, flags_t annex, bool fixed_point, Phases&& phases)
    : Phase(world, annex)
    , fixed_point_(fixed_point)
    , phases_(std::move(phases)) {
    name_ += fixed_point_ ? " tt" : " ff";
}

void PhaseMan::start() {
    int iter = 0;
    for (bool todo = true; todo; ++iter) {
        todo = false;

        if (fixed_point()) VLOG("fixed-point iteration: {}", iter);

        for (auto& phase : phases()) {
            phase->run();
            todo |= phase->todo();
        }

        todo &= fixed_point();

        if (todo)
            for (auto& old_phase : phases()) {
                auto new_phase = std::unique_ptr<Phase>(old_phase->recreate().release()->as<Phase>());
                swap(old_phase, new_phase);
            }

        todo_ |= todo;
    }
}

/*
 * PassManPhase
 */

PassManPhase::PassManPhase(World& world, flags_t annex, std::unique_ptr<Pass>&& pass)
    : Phase(world, annex)
    , man_(std::make_unique<PassMan>(world, annex)) {
    man_->add(std::move(pass));
}

} // namespace mim
