#include "dialects/autodiff/analysis/affine_dfa.h"

#include "dialects/autodiff/analysis/analysis.h"
#include "dialects/autodiff/analysis/analysis_factory.h"
#include "dialects/mem/autogen.h"

namespace thorin::autodiff {

AffineDFA::AffineDFA(AnalysisFactory& factory)
    : Analysis(factory) {
    run();
}

void AffineDFA::run() {
    for (auto cfa_node : factory().cfa().post_order()) {
        if (auto lam = cfa_node->def()->isa_nom<Lam>()) { run(lam); }
    }
}

} // namespace thorin::autodiff
