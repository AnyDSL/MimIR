#include "mim/phase.h"

#include "mim/driver.h"

namespace mim {

static auto apply(World& world, const Flags2Phases& phases, const Def* app) {
    auto p_def = App::uncurry_callee(app);
    world.DLOG("apply pass/phase: `{}`", p_def);

    if (auto axm = p_def->isa<Axm>())
        if (auto i = phases.find(axm->flags()); i != phases.end())
            return i->second(world);
        else
            error("pass/phase `{}` not found", axm->sym());
    else
        error("unsupported callee for a phase/pass: `{}`", p_def);
}

std::unique_ptr<Phase> Phase::recreate() {
    auto ctor = world().driver().phase(annex_);
    auto ptr  = (*ctor)(world());
    ptr->inherit(*this);
    return ptr;
}

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

PhaseMan::PhaseMan(World& world, bool fixed_point)
    : Phase(world, std::format("pipeline (fixed_point_: `{}`)", fixed_point))
    , fixed_point_(fixed_point) {}

void PhaseMan::set(const App* app) {
    if (annex() == Annex::Base<compile::phases>) {
        auto [fp, arg] = app->uncurry_args<2>();
        fixed_point_   = Lit::as<bool>(fp);
        for (auto def : arg->projs())
            if (auto phase = apply(world(), world().driver().phases(), def)) add(std::move(phase));
    } else {
        auto defs = app->as<App>()->arg()->projs();
        auto man  = std::make_unique<PassMan>(app->world());
        for (auto def : defs)
            apply(man->world(), passes, *man, def);
        return std::make_unique<PassManPhase>(world, std::move(man));
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
                    new_phase->inherit(*phase);
                    swap(new_phase, phase);
                }
            }

            todo_ |= todo;
        }
    }

} // namespace mim
