#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/auxiliary/autodiff_cache_optimizer.h"
#include "dialects/autodiff/auxiliary/autodiff_dep_analysis.h"
#include "dialects/autodiff/auxiliary/autodiff_flow_analysis.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

class CacheAnalysis {
public:
    DefSet requirements;
    DefSet requirements_filtered;
    DefSet targets_;

    FlowAnalysis flow_analysis;
    WARAnalysis war_analysis;
    CacheOptimizer cache_optimizer;
    explicit CacheAnalysis(Lam* lam)
        : flow_analysis(lam)
        , war_analysis(lam)
        , cache_optimizer(lam) {
        run();
    }

    FlowAnalysis& flow() { return flow_analysis; }

    DefSet& targets() { return targets_; }

    bool requires_caching(const Def* def) { return targets_.contains(def); }

    void require(const Def* def) { 
        if (!def->isa<Lit>()) {
            requirements.insert(def);
        }
    }

    void visit(const Def* def) {
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

        if (auto tri = match<math::tri>(def)) {
            require(tri->arg());
        }

        if (auto lea = match<mem::lea>(def)) {
            auto index = lea->arg(1);
            if (contains_load(index)) { require(index); }
        }

        if (auto gamma = match<math::gamma>(def)) { require(gamma->arg(0)); }
    }

    void filter(const Def* def) {
        if (auto arith = match<math::arith>(def)) {
            auto [left, right] = arith->args<2>();
            filter(left);
            filter(right);
        } else if (auto wrap = match<core::wrap>(def)) {
            auto [left, right] = wrap->args<2>();
            filter(left);
            filter(right);
        } else if (auto conv = match<math::conv>(def)) {
            filter(conv->arg());
        } else if (auto conv = match<core::conv>(def)) {
            filter(conv->arg());
        } else if (auto app = match<math::exp>(def)) {
            filter(app->arg());
        } else if (auto app = match<math::gamma>(def)) {
            filter(app->arg());
        } else {
            requirements_filtered.insert(def);
        }
    }

    void filter() {
        for (auto requirement : requirements) { filter(requirement); }
    }

    void run() {
        for (auto flow_def : flow_analysis.flow_defs()) { visit(flow_def); }
        //filter();
        targets_ = cache_optimizer.optimize(requirements);
        //filter();
        /*for (auto requirement : requirements_filtered) {
            if (auto load = is_load_val(requirement)) {
                if (war_analysis.is_overwritten(requirement)) { targets_.insert(requirement); }
            } else if (!requirement->isa<Lit>() && !isa_nested_var(requirement)) {
                targets_.insert(requirement);
            }
        }*/
        /*auto old_size = targets_.size();
        targets_      = cache_optimizer.optimize(requirements);
        auto new_size = targets_.size();*/

/*
        for (auto requirement : requirements) {
            if (!requirement->isa<Lit>() && !isa_nested_var(requirement)) {
                targets_.insert(requirement);
            }
        }*/
    }

    bool isa_nested_var(const Def* def) {
        if (auto extract = def->isa<Extract>()) {
            return isa_nested_var(extract->tuple());
        } else if (def->isa<Var>()) {
            return true;
        }

        return false;
    }

    const App* is_load_val(const Def* def) {
        if (auto extr = def->isa<Extract>()) {
            auto tuple = extr->tuple();
            if (auto load = match<mem::load>(tuple)) { return load; }
        }

        return nullptr;
    }

    bool contains_load(const Def* def) {
        if (def->isa<Var>()) return false;
        if (match<mem::load>(def)) return true;

        for (auto op : def->ops()) {
            if (contains_load(op)) { return true; }
        }

        return false;
    }
};

} // namespace thorin::autodiff
