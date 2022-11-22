#include "dialects/autodiff/auxiliary/affine_dfa.h"

#include "thorin/def.h"
#include "thorin/tuple.h"

#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/affine_cfa.h"
#include "dialects/autodiff/auxiliary/analysis.h"
#include "dialects/autodiff/auxiliary/analysis_factory.h"
#include "dialects/autodiff/builder.h"
#include "dialects/math/math.h"
#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

AffineDFA::AffineDFA(AnalysisFactory& factory)
    : Analysis(factory) {
    run();
    // run(factory.lam());
}

void AffineDFA::run() {
    for (auto cfa_node : factory().cfa().post_order()) {
        if (auto lam = cfa_node->def()->isa_nom<Lam>()) { run(lam); }
    }
}

} // namespace thorin::autodiff
