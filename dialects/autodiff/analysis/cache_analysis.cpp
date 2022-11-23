
#include "dialects/autodiff/analysis/cache_analysis.h"

#include "thorin/def.h"
#include "thorin/tuple.h"

#include "dialects/autodiff/analysis/analysis_factory.h"
#include "dialects/autodiff/analysis/cache_optimizer.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/utils/builder.h"
#include "dialects/math/math.h"
#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

CacheAnalysis::CacheAnalysis(AnalysisFactory& factory)
    : Analysis(factory)
    , alias(factory.alias()) {
    run();
}

void CacheAnalysis::run() {
    auto& utils    = factory().utils();
    auto& gradient = factory().gradient();
    auto& war      = factory().war();
    auto& live     = factory().live();

    for (auto def : gradient.defs()) { visit(def); }

    CacheOptimizer cache_optimizer(factory());
    targets_ = cache_optimizer.optimize(requirements);

    for (auto requirement : requirements) { depends_on_loads(requirement, targets_, loads_); }

    for (auto load : loads_) {
        Lam* lam = live.end_of_live(load);
        assert(lam);
        lam2loads_[lam].insert(load);
    }
}

void CacheAnalysis::depends_on_loads(const Def* def, DefSet& set, DefSet& loads) {
    def = alias.get(def);

    if (set.contains(def)) {
        return;
    } else if (auto load = is_load_val(def)) {
        loads.insert(load);
    } else if (auto app = def->isa<App>()) {
        depends_on_loads(app->arg(), set, loads);
    } else if (auto tuple = def->isa<Tuple>()) {
        for (auto op : tuple->ops()) { depends_on_loads(op, set, loads); }
    }
}

DefSet& CacheAnalysis::requires_loading(Lam* lam) { return lam2loads_[lam]; }
bool CacheAnalysis::requires_caching(const Def* def) { return targets_.contains(def); }

void CacheAnalysis::require(const Def* def) {
    def = alias.get(def);
    requirements.insert(def);
}

void CacheAnalysis::visit(const Def* def) {
    if (auto rop = match<math::arith>(def)) {
        if (rop.id() == math::arith::mul) {
            require(rop->arg(0));
            require(rop->arg(1));
        } else if (rop.id() == math::arith::div) {
            require(rop->arg(0));
            require(rop->arg(1));
        }
    } else if (auto extrema = match<math::extrema>(def)) {
        if (extrema.id() == math::extrema::maximum || extrema.id() == math::extrema::minimum) {
            require(extrema->arg(0));
            require(extrema->arg(1));
        }
    }

    if (auto exp = match<math::exp>(def)) {
        if (exp.id() == math::exp::exp) {
            require(exp);
        } else if (exp.id() == math::exp::log) {
            require(exp->arg());
        }
    }

    if (auto rt = match<math::rt>(def)) { require(rt); }
    if (auto tri = match<math::tri>(def)) { require(tri->arg()); }

    if (auto lea = match<mem::lea>(def)) { require(lea->arg(1)); }

    if (auto gamma = match<math::gamma>(def)) { require(gamma->arg(0)); }
}

} // namespace thorin::autodiff
