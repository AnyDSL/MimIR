#include "dialects/autodiff/auxiliary/affine_cfa.h"

#include "thorin/def.h"
#include "thorin/tuple.h"

#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/analysis.h"
#include "dialects/autodiff/auxiliary/analysis_factory.h"
#include "dialects/autodiff/builder.h"
#include "dialects/math/math.h"
#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

AffineCFA::AffineCFA(AnalysisFactory& factory)
    : Analysis(factory) {
    run(factory.lam());
}

} // namespace thorin::autodiff
