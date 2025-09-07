#include "mim/phase.h"

#include "mim/driver.h"

namespace mim {

/*
 * Phase
 */

Phase::Phase(World& world, flags_t annex)
    : world_(world)
    , annex_(annex)
    , name_(world.annex(annex)->sym()) {}

std::unique_ptr<Phase> Phase::recreate() {
    auto ctor = driver().phase(annex());
    auto ptr  = (*ctor)(world());
    ptr->apply(*this);
    return ptr;
}

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

void PhaseMan::apply(bool fp, Phases&& phases) {
    fixed_point_ = fp;
    phases_      = std::move(phases);
    name_ += fixed_point_ ? " tt" : " ff";
}

void PhaseMan::apply(const App* app) {
    auto [fp, args] = app->uncurry_args<2>();

    auto phases = Phases();
    for (auto arg : args->projs())
        if (auto phase = create(driver().phases(), *this, arg)) phases.emplace_back(std::move(phase));

    apply(Lit::as<bool>(fp), std::move(phases));
}

void PhaseMan::apply(Phase& phase) {
    auto& man = static_cast<PhaseMan&>(phase);
    apply(man.fixed_point(), std::move(man.phases_));
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

        if (todo) {
            for (auto& old_phase : phases()) {
                auto new_phase = old_phase->recreate();
                swap(new_phase, old_phase);
            }
        }

        todo_ |= todo;
    }
}

/*
 * PassManPhase
 */

std::unique_ptr<Phase> PassManPhase::recreate() {
    error("Recreate not supported for Passes and doesn't really make sense as they are already run in a fixed-point");
}

void PassManPhase::apply(const App* app) {
    man_ = std::make_unique<PassMan>(world());
    for (auto arg : app->args())
        Phase::create(driver().passes(), *man_, arg);
}

void PassManPhase::apply(Phase& phase) {
    assert(false);
    auto& pmp = static_cast<PassManPhase&>(phase);
    man_      = std::move(pmp.man_);
}

} // namespace mim
