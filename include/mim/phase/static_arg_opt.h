#pragma once

#include "mim/phase.h"

namespace mim {

/// Static Argument Transformation.
/// @see [Compilation by Transformation in Non-Strict Functional Languages,
/// §7](https://theses.gla.ac.uk/74568/1/10992188.pdf)
class StaticArgOpt : public RWPhase {
public:
    StaticArgOpt(World& world, flags_t annex)
        : RWPhase(world, annex) {
        assert(world.flags().scalarize_threshold < 64 && "todo: need dynamic bitset");
    }

private:
    bool analyze() final;
    void analyze(const Def*);
    void visit(const Def*, bool candidate = true); // lattice: true -> false

    const Def* rewrite_imm_App(const App*) final;

    DefSet analyzed_;
    LamMap<uint64_t> lam2statics_;
    LamMap<std::pair<Lam*, Lam*>> lam2ph_;
};

} // namespace mim
