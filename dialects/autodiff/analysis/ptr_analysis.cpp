#include "dialects/autodiff/analysis/ptr_analysis.h"

#include "thorin/def.h"
#include "thorin/tuple.h"

#include "dialects/autodiff/analysis/analysis.h"
#include "dialects/autodiff/analysis/analysis_factory.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/builder.h"
#include "dialects/math/math.h"
#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

PtrAnalysis::PtrAnalysis(AnalysisFactory& factory)
    : Analysis(factory) {
    run();
}

UnionNode<const Def*>* PtrAnalysis::ptr_node(const Def* def) {
    def = factory().alias().get(def);

    auto i = ptr_union.find(def);
    if (i == ptr_union.end()) {
        auto p = ptr_union.emplace(def, std::make_unique<UnionNode<const Def*>>(def));
        assert_unused(p.second);
        i = p.first;
    }
    return &*i->second;
}

void PtrAnalysis::run() {
    for (auto def : factory().dfa().ops()) {
        if (auto lea = match<mem::lea>(def)) {
            auto arg = lea->arg();
            auto arr = arg->proj(0);
            auto idx = arg->proj(1);
            unify(ptr_node(arr), ptr_node(lea));
        } else if (auto bitcast = match<core::bitcast>(def)) {
            auto ptr = bitcast->arg();
            unify(ptr_node(bitcast), ptr_node(ptr));
        }
    }
}
} // namespace thorin::autodiff
