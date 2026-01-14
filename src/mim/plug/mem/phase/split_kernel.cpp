#include "mim/plug/mem/phase/split_kernel.h"

#include "mim/phase.h"

#include "mim/plug/mem/mem.h"

namespace mim::plug::mem::phase {

void SplitKernel::start() {
    analyze();

    for (const auto& [f, def] : old_world().flags2annex())
        rewrite_annex(f, def);

    bootstrapping_ = false;

    for (auto kernel : kernels_)
        rewrite(kernel);
}

bool SplitKernel::analyze() {
    for (auto def : old_world().annexes())
        analyze(def);
    for (auto def : old_world().externals().muts())
        analyze(def);

    return false; // no fixed-point neccessary
}

void SplitKernel::analyze(const Def* def) {
    if (auto [_, ins] = analyzed_.emplace(def); !ins) return;
    if (def->isa<Var>()) return; // ignore Var's mut

    if (auto launch = Axm::isa<mem::launch>(def)) {
        auto [mem0, n_warps, n_threads, kernel] = launch->args<4>();
        if (auto lam = kernel->isa_mut<Lam>()) kernels_.emplace(lam);
    }

    for (auto d : def->deps())
        analyze(d);
}

const Def* SplitKernel::rewrite_mut_Lam(Lam* lam) {
    auto res = RWPhase::rewrite_mut_Lam(lam);
    if (kernels_.contains(lam)) {
        res->as_mut<Lam>()->externalize();
        lam->unset();
    }

    return res;
}

} // namespace mim::plug::mem::phase
