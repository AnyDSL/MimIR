
#include "dialects/autodiff/auxiliary/autodiff_cache_analysis.h"

#include "thorin/def.h"
#include "thorin/tuple.h"

#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/analysis_factory.h"
#include "dialects/autodiff/auxiliary/autodiff_cache_optimizer.h"
#include "dialects/autodiff/builder.h"
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
    auto& utils = factory().utils();
    auto& flow  = factory().flow();
    auto& war   = factory().war();
    auto& live  = factory().live();

    // for (auto flow_def : flow_analysis.flow_defs()) { visit(flow_def); }
    for (auto def : flow.flow_defs()) { visit(def); }

    // filter();

    CacheOptimizer cache_optimizer(factory());
    targets_ = cache_optimizer.optimize(requirements);
    // loads_   = cache_optimizer.loads();

    for (auto requirement : requirements) { depends_on_loads(requirement, targets_, loads_); }

    auto load_size   = loads_.size();
    auto end_of_live = live.end_of_live();
    auto test_size   = end_of_live.size();
    std::cout << load_size << std::endl;
    std::cout << test_size << std::endl;

    for (auto load : loads_) {
        Lam* lam = live.end_of_live(load);
        assert(lam);
        lam2loads_[lam].insert(load);
    }

    //   filter();
    /*
    for (auto requirement : requirements_filtered) {
        auto target = alias.get(requirement);
        if (factory().utils().is_loop_index(target)) continue;
        if (requirement->isa<Lit>()) continue;
        if (utils.is_root_var(target)) continue;
        if (is_load_val(target) && !war.is_overwritten(target)) continue;
        targets_.insert(target);
    }

    for (auto target : targets_) {
        target->dump(1);
        target->dump(1);
    }
    */
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

void CacheAnalysis::filter(const Def* def) {
    def = factory().alias().get(def);
    if (is_load_val(def)) {
        requirements_filtered.insert(def);
    } else if (is_var(def)) {
        if (factory().utils().is_loop_index(def)) return;
        requirements_filtered.insert(def);
    } else if (auto app = def->isa<App>()) {
        filter(app->arg());
    } else if (def->num_projs() > 1) {
        for (auto proj : def->projs()) { filter(proj); }
    } else if (def->isa<Lit>()) {
        return;
    } else {
        assert(false);
    }
}

void CacheAnalysis::filter() {
    for (auto requirement : requirements) { filter(requirement); }
}

} // namespace thorin::autodiff
