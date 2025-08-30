#include "mim/phase/eta_exp_phase.h"

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
        outln("analyze: {}", def);
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

const Def* EtaExpPhase::eta(const Def* def) {
    auto new_def = rewrite(def);
    if (auto lam = def->isa_mut<Lam>())
        if (auto l = lattice(lam); l == Both) return Lam::eta_expand(false, new_def);
    return new_def;
}

const Def* EtaExpPhase::rewrite_imm_App(const App* app) {
    auto callee = rewrite(app->callee());
    return new_world().app(callee, eta(app->arg()));
}

const Def* EtaExpPhase::rewrite_mut_Lam(Lam* lam) {
    auto new_def = Rewriter::rewrite_mut_Lam(lam);
    if (auto l = lattice(lam); l == Both) return Lam::eta_expand(false, new_def);
    return new_def;
}

} // namespace mim
