#include "mim/plug/mem/phase/split_kernel.h"

#include "mim/phase.h"

#include "mim/plug/mem/mem.h"

namespace mim::plug::mem::phase {

void SplitKernel::start() {
    analyze();

    for (const auto& [f, def] : old_world().flags2annex())
        rewrite_annex(f, def);

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

    if (auto launch = Axm::isa<mem::launch>(def)) {
        auto [mem, n_warps, n_threads, kernel] = launch->args<4>();
        if (auto lam = kernel->isa_mut<Lam>()) kernels_.emplace(lam);
    }

    for (auto d : def->deps())
        analyze(d);
}

const Def* SplitKernel::rewrite_mut_Lam(Lam* old_lam) {
    auto new_def = RWPhase::rewrite_mut_Lam(old_lam);

    if (kernels_.contains(old_lam)) {
        new_def->as_mut<Lam>()->externalize();
        old_lam->unset();
    }

    return new_def;
}

} // namespace mim::plug::mem::phase
