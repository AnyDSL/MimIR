#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

class FlowAnalysis {
public:
    DefSet flow_set;
    //std::vector<std::pair<const Def*, const Def*>> reasoning_list;

    DefSet& flow_defs() { return flow_set; }

    bool isa_flow_def(const Def* def) { return flow_set.contains(def); }

    bool add(const Def* present, const Def* next) {
        if (flow_set.contains(present)) {
            //reasoning_list.push_back({present, next});
            flow_set.insert(next);
            return true;
        } else {
            return false;
        }
    }
    
    bool add_projs(const Def* present, const Def* next) {
        if (flow_set.contains(present)) {
            for (auto proj : next->projs()) { flow_set.insert(proj); }
            return true;
        } else {
            return false;
        }
    }

    bool visit(const Def* def) {
        if (auto tuple = def->isa<Tuple>()) {
            return add_projs(tuple, tuple);
        } else if (auto pack = def->isa<Pack>()) {
            return add(pack, pack->body());
        } else if (auto app = def->isa<App>()) {
            auto arg = app->arg();

            if (match<math::arith>(app) || match<math::extrema>(app) || match<math::exp>(app) ||
                match<math::gamma>(app)) {
                return add(app, arg);
            } else if (auto lea = match<mem::lea>(app)) {
                auto arr = arg->proj(0);
                auto idx = arg->proj(1);
                return add(lea, arr) || add(arr, lea);
            } else if (auto store = match<mem::store>(app)) {
                auto ptr = arg->proj(1);
                auto val = arg->proj(2);

                return add(ptr, val);
            } else if (auto load = match<mem::load>(app)) {
                auto val = load->proj(1);
                auto ptr = arg->proj(1);
                return add(val, ptr);
            } else if (auto bitcast = match<core::bitcast>(app)) {
                return add(bitcast, bitcast->arg());
            }

            const Def* exit = nullptr;
            if (auto for_affine = match<affine::For>(app)) {
                exit = for_affine->arg(5);
            } else {
                exit = app->callee();
                return add_projs(exit, arg);
            }

        }

        return true;
    }

    void run(Lam* diffee) {
        Scope scope(diffee);
        auto ret_var = diffee->ret_var();

        flow_set.insert(ret_var);
        flow_set.insert(diffee->var());
        if (diffee->num_vars() > 1) {
            for (auto var : diffee->vars()) { flow_set.insert(var); }
        }

        DefSet state[2];
        state[0]          = scope.bound();
        bool dirty        = true;
        int current_state = 0;
        while (dirty) {
            dirty                = false;
            int next_state       = 1 - current_state;
            DefSet& current_defs = state[current_state];
            for (auto def : current_defs) {
                bool finished = visit(def);

                if (finished) {
                    dirty = true;
                } else {
                    state[next_state].insert(def);
                }
            }

            state[current_state].clear();
            current_state = next_state;
        }

        /*for (auto [prev, next] : reasoning_list) {
            prev->dump(1);
            next->dump(1);
        }*/
    }
};

} // namespace thorin::autodiff
