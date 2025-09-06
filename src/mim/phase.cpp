#include "mim/phase.h"

#include "mim/driver.h"

namespace mim {

auto do_apply(const Def* def) {
    World& world = def->world();
    auto& phases = world.driver().phases();
    auto p_def   = App::uncurry_callee(def);
    world.DLOG("apply pass/phase: `{}`", p_def);

    if (auto axm = p_def->isa<Axm>())
        if (auto i = phases.find(axm->flags()); i != phases.end()) {
            auto phase = i->second(world);
            if (auto app = def->isa<App>()) phase->apply(app);
            return phase;
        } else
            error("pass/phase `{}` not found", axm->sym());
    else
        error("unsupported callee for a phase/pass: `{}`", p_def);
}

auto do_apply(PassMan& man, const Def* app) {
    World& world = app->world();
    auto& passes = world.driver().passes();
    auto p_def   = App::uncurry_callee(app);
    app->world().DLOG("apply pass/phase: `{}`", p_def);

    if (auto axm = p_def->isa<Axm>())
        if (auto i = passes.find(axm->flags()); i != passes.end())
            return i->second(man, app);
        else
            error("pass/phase `{}` not found", axm->sym());
    else
        error("unsupported callee for a phase/pass: `{}`", p_def);
}

/*
 * Phase
 */

Phase::Phase(World& world, flags_t annex)
    : world_(world)
    , annex_(annex)
    , name_(world.annex(annex)->sym()) {}

std::unique_ptr<Phase> Phase::recreate() {
    auto ctor = world().driver().phase(annex_);
    auto ptr  = (*ctor)(world());
    ptr->apply(*this);
    return ptr;
}

void Phase::run() {
    world().verify().ILOG("=== Phase start: {} ===", name());
    start();
    world().verify().ILOG("=== Phase done:  {} ===", name());
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

void RWPhase::rewrite_external(Def* mut) { mut->transfer_external(rewrite(mut)->as_mut()); }

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
    auto [fp, arg] = app->uncurry_args<2>();

    auto phases = Phases();
    for (auto def : arg->projs())
        if (auto phase = do_apply(def)) phases.emplace_back(std::move(phase));

    apply(Lit::as<bool>(fp), std::move(phases));
}

void PhaseMan::apply(Phase& phase) {
    auto& man = static_cast<PhaseMan&>(phase);
    apply(man.fixed_point(), std::move(man.phases_));
}

void PhaseMan::start() {
    for (bool todo = true; todo;) {
        todo = false;
        for (auto& phase : phases()) {
            phase->run();
            todo |= phase->todo();
        }

        todo &= fixed_point();

        if (todo) {
            for (auto& phase : phases()) {
                auto new_phase = phase->recreate();
                new_phase->apply(*phase);
                swap(new_phase, phase);
            }
        }

        todo_ |= todo;
    }
}

/*
 * PassManPhase
 */

void PassManPhase::apply(const App* app) {
    man_ = std::make_unique<PassMan>(app->world());
    for (auto arg : app->args())
        do_apply(*man_, arg);
}

void PassManPhase::apply(Phase& phase) {
    assert(false);
    auto& pmp = static_cast<PassManPhase&>(phase);
    man_      = std::move(pmp.man_);
}

} // namespace mim
