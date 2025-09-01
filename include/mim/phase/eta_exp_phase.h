#pragma once

#include "mim/phase/phase.h"
#include "mim/util/util.h"

namespace mim {

/// Inlines in post-order all Lam%s that occur exactly *once* in the program.
class EtaExpPhase : public FPPhase {
public:
    EtaExpPhase(World&);

private:
    enum Lattice : u8 {
        None    = 0,
        Known   = 1 << 0,
        Unknown = 1 << 1,
        Both    = Known | Unknown,
    };

    static void join(Lattice& l1, Lattice l2) { l1 = (Lattice)((u8)l1 | (u8)l2); }

    void join(const Lam* lam, Lattice l) {
        if (auto [i, ins] = lam2lattice_.emplace(lam, l); !ins) join(i->second, l);
    }

    Lattice lattice(const Lam* lam) {
        if (auto i = lam2lattice_.find(lam); i != lam2lattice_.end()) return i->second;
        return None;
    }

    void reset() final;

    bool analyze() final;
    void analyze(const Def*);

    void rewrite_external(Def*) final;
    const Def* rewrite_no_eta(const Def*);
    const Def* rewrite(const Def*);
    const Def* rewrite_imm_App(const App*) final;
    const Def* rewrite_imm_Var(const Var*) final;

    DefSet analyzed_;
    GIDMap<const Lam*, Lattice> lam2lattice_;
    GIDMap<const Lam*, const Def*> lam2eta_;
};

} // namespace mim
