#include "mim/phase/eta_exp_phase.h"

#include "mim/rewrite.h"

namespace mim {

EtaExpPhase::EtaExpPhase(World& world)
    : FPPhase(world, "eta_exp") {}

void EtaExpPhase::reset() {
    analyzed_.clear();
    lam2lattice_.clear();
}

bool EtaExpPhase::analyze() {
    for (auto def : old_world().externals()) {
        if (auto lam = def->isa<Lam>()) join(lam, Lattice::Known);
        analyze(def);
    }

    return false; // no fixed-point neccessary
}

void EtaExpPhase::analyze(const Def* def) {
    if (auto [_, ins] = analyzed_.emplace(def); !ins) return;

    if (auto var = def->isa<Var>()) return analyze(var->type()); // ignore Var's mut

    if (auto app = def->isa<App>()) {
        // clang-format off
        if (auto lam = app->  type()->isa_mut<Lam>()) join(lam, Lattice::Unknown);
        if (auto lam = app->callee()->isa_mut<Lam>()) join(lam, Lattice::  Known);
        if (auto lam = app->   arg()->isa_mut<Lam>()) join(lam, Lattice::Unknown);
        // clang-format on
        analyze(app->type());
        analyze(app->callee());
        analyze(app->arg());
    } else {
        for (auto d : def->deps()) {
            if (auto lam = d->isa_mut<Lam>()) {
                if (def->isa<App>()) join(lam, Lattice::Unknown);
            }
            analyze(d);
        }
    }
}

void EtaExpPhase::rewrite_external(Def* mut) { mut->transfer_external(rewrite_no_eta(mut)->as_mut()); }

const Def* EtaExpPhase::rewrite(const Def* old_def) {
    if (auto lam = old_def->isa<Lam>(); lam && lattice(lam) == Both) {
        auto [i, ins] = lam2eta_.emplace(lam, nullptr);
        if (ins) i->second = Lam::eta_expand(rewrite_no_eta(lam));
        return i->second;
    }

    return Rewriter::rewrite(old_def);
}

const Def* EtaExpPhase::rewrite_no_eta(const Def* old_def) { return Rewriter::rewrite(old_def); }

const Def* EtaExpPhase::rewrite_imm_App(const App* app) {
    auto callee = rewrite_no_eta(app->callee());
    return new_world().app(callee, rewrite(app->arg()));
}

const Def* EtaExpPhase::rewrite_imm_Var(const Var* var) {
    return new_world().var(rewrite(var->type()), rewrite_no_eta(var->mut())->as_mut());
}

} // namespace mim
