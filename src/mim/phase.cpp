#include "mim/phase.h"

#include <memory>

#include "mim/driver.h"

namespace mim {

/*
 * Phase
 */

void Phase::run() {
    world().verify().ILOG("=== Phase start: `{}` ===", name());
    start();
    world().verify().ILOG("=== Phase done:  `{}` ===", name());
}

/*
 * RWPhase
 */

void RWPhase::start() {
    for (const auto& [f, def] : old_world().flags2annex())
        rewrite_annex(f, def);

    bootstrapping_ = false;

    for (auto mut : old_world().copy_externals())
        rewrite_external(mut);

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

#if 0
void PhaseMan::apply(const App* app) {
    auto [fp, args] = app->uncurry_args<2>();

    auto phases = Phases();
    for (auto arg : args->projs())
        if (auto stage = create(driver().stages(), arg)) {
            if (auto pmp = stage->isa<PassManPhase>(); pmp && pmp->man().empty()) continue;
            phases.emplace_back(std::unique_ptr<Phase>(static_cast<Phase*>(stage.release())));
        }

    apply(Lit::as<bool>(fp), std::move(phases));
}
#endif

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

#if 0
void PassManPhase::apply(const App* app) {
    man_        = std::make_unique<PassMan>(world(), annex());
    auto passes = Passes();
    for (auto arg : app->args())
        if (auto stage = Phase::create(driver().stages(), arg))
            passes.emplace_back(std::unique_ptr<Pass>(static_cast<Pass*>(stage.release())));
    man_->apply(std::move(passes));
}
#endif

} // namespace mim
