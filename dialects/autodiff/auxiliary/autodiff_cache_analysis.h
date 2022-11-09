#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/auxiliary/autodiff_flow_analysis.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

class CacheAnalysis {
public:
    DefSet requirements;
    DefSet requirements_filtered;
    DefSet targets_;

    FlowAnalysis& flow_analysis;
    explicit CacheAnalysis(FlowAnalysis& flow_analysis)
        : flow_analysis(flow_analysis) {}

    DefSet& targets() { return targets_; }

    bool requires_caching(const Def* def){
        return targets_.contains(def);
    }

    void require(const Def* def) { requirements.insert(def); }

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
                require(exp->arg(0));
            }
        }

        if (auto lea = match<mem::lea>(def)) {
            lea->dump(1);
            auto index = lea->arg(1);
            if (contains_load(index)) { require(index); }
        }

        if (auto gamma = match<math::gamma>(def)) { require(gamma->arg(0)); }
    }

    void filter(const Def* def) {
        if(auto arith = match<math::arith>(def)){
            auto [left, right] = arith->args<2>();

            filter(left);
            filter(right);
        }else{
            requirements_filtered.insert(def);
        }
    }

    void filter() {
        for (auto requirement : requirements) { filter(requirement); }
    }

    void run() {
        for (auto flow_def : flow_analysis.flow_defs()) { visit(flow_def); }

        filter();

        for (auto requirement : requirements_filtered) {
            if (auto load = is_load_val(requirement)) {
                auto ptr = load->arg(1);
                if (has_op_store(ptr)) { targets_.insert(requirement); }
            } else if (!requirement->isa<Lit>()) {
                targets_.insert(requirement);
            }
        }
    }

    const App* is_load_val(const Def* def) {
        if (auto extr = def->isa<Extract>()) {
            auto tuple = extr->tuple();
            if (auto load = match<mem::load>(tuple)) { return load; }
        }

        return nullptr;
    }

    bool has_op_store(const Def* def, DefSet& visited) {
        if (visited.contains(def)) return false;
        visited.insert(def);

        if (auto extr = def->isa<Extract>()) {
            if (has_op_store(extr->tuple(), visited)) { return true; }
        } else if (auto lea = match<mem::lea>(def)) {
            auto [arr_ptr, _] = lea->args<2>();

            if (has_op_store(arr_ptr, visited)) { return true; }
        } else if (match<mem::store>(def)) {
            return true;
        }

        if (match<mem::Ptr>(def->type()) || def->isa<Tuple>()) {
            for (auto use : def->uses()) {
                if (has_op_store(use, visited)) { return true; }
            }
        }

        return false;
    }

    bool has_op_store(const Def* ptr) {
        DefSet visited;
        return has_op_store(ptr, visited);
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
