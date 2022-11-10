#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/auxiliary/autodiff_lea_analysis.h"
#include "dialects/autodiff/auxiliary/autodiff_war_analysis.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {


struct CacheState{
    std::queue<const Def*> queue;
    DefSet cached_defs;
    DefSet computed_defs;

    bool is_better_than(std::shared_ptr<CacheState>& other){
        auto this_caches = cached_defs.size();
        auto other_caches = other->cached_defs.size();
        if(this_caches < other_caches) return true;
        if(this_caches > other_caches) return false;
        return computed_defs.size() <= other->computed_defs.size();
    }
};

template<class T>
size_t hash_of_set(T set){
    size_t hash = 0;

    for( auto def : set ){
        hash ^= GIDHash<const Def*>()(def);
    }

    return hash;
}

struct CacheStateHash {
    size_t operator()(std::shared_ptr<CacheState> p) const { 
        return hash_of_set(p->cached_defs) ^ ( hash_of_set(p->computed_defs) * 31 );
    };
};

struct CacheStateEq {
    bool operator()(std::shared_ptr<CacheState> a, std::shared_ptr<CacheState> b) const { 
        return a->cached_defs == b->cached_defs && a->computed_defs == b->computed_defs; 
    }
};


class CacheOptimizer {
public:

    absl::flat_hash_set<std::shared_ptr<CacheState>, CacheStateHash, CacheStateEq> explored;
    //std::unordered_set<std::shared_ptr<CacheState>> explored;
    std::shared_ptr<CacheState> best;
    DepAnalysis dep_analysis;
    WARAnalysis war_analysis;
    std::vector<DefSet> groups;
    CacheOptimizer(Lam* lam) : dep_analysis(lam), war_analysis(lam), best(nullptr){}

    void explore(std::shared_ptr<CacheState>& last, const Def* removed, const Def* parts){
        auto next_state = std::make_shared<CacheState>(*last);

        next_state->cached_defs.erase(removed);
        if(next_state->computed_defs.insert(removed).second){
            for( auto def : parts->projs() ){
                if(!next_state->computed_defs.contains(def)){
                    if(needs_cache(def)){
                        auto result = next_state->cached_defs.insert(def);

                        if(result.second && !requires_cache(def)){
                            next_state->queue.push(def);
                        }
                    }
                }
            }
        }

        if(explored.insert(next_state).second){
            if(!best->is_better_than(next_state)){
                std::cout << next_state->cached_defs.size() << std::endl;
                best = next_state;
            }

            search(next_state);
        }
    }
/*
    void find(const Def* def){
        def->dump(1);
        std::unordered_set<Node*> visited;
        auto node = dep_analysis.node(def);

        find(node, visited);

        for( auto node : visited ){
            node->def->dump(1);
        }
    }   */

    bool is_flow_op(const Def* def){
        return match<math::arith>(def) || 
            match<core::wrap>(def) || 
            match<math::conv>(def) || 
            match<core::conv>(def) || 
            match<math::exp>(def) || 
            match<math::gamma>(def) || 
            def->isa<Tuple>() || 
            def->isa<Pack>()|| 
            def->isa<Extract>()|| 
            match<mem::load>(def);
    }

    void canonicalize(DefSet defs){
        while(defs.size() > 0){
            const Def* def = *defs.begin();
            DefSet visited;
            auto node = dep_analysis.node(def);
            find(node, visited);

            DefSet& canonical_group = groups.emplace_back();
            for( auto visited_def : visited ){
                visited_def->dump(1);
                if(defs.contains(visited_def)){
                    defs.erase(visited_def);
                    canonical_group.insert(visited_def);
                }
            }
        }
    }

    void find(Node* node, DefSet& visited){
        //if(!is_flow_op(node->def) && !node->def->isa<Tuple>() && !node->def->isa<Pack>()) return;
        if(!is_flow_op(node->def)) return;
        if(!visited.insert(node->def).second) return;
        if(!is_load_val(node->def)){
            for( auto succ : node->succs ){
                find(succ, visited);
            }
        }

        if(is_load_val(node->def) && !war_analysis.is_overwritten(node->def)){
            return;
        }

        for( auto prev : node->preds ){
            find(prev, visited);
        }
    }

    void search(std::shared_ptr<CacheState> current){
        auto& queue = current->queue;
        for(  ; queue.size() > 0 ;  ){
            auto def = queue.front();
            queue.pop();
            if(auto app = match<math::arith>(def)){
                explore(current, def, app->arg());
            } else if (auto app = match<core::wrap>(def)) {
                explore(current, def, app->arg());
            } else if (auto app = match<math::conv>(def)) {
                explore(current, def, app->arg());
            } else if (auto app = match<core::conv>(def)) {
                explore(current, def, app->arg());
            } else if (auto app = match<math::exp>(def)) {
                explore(current, def, app->arg());
            } else if (auto app = match<math::gamma>(def)) {
                explore(current, def, app->arg());
            }
        }
    }

    bool requires_cache(const Def* def){
        if (is_load_val(def) && war_analysis.is_overwritten(def)) {
            return true;
        }

        return false;
    }

    bool needs_cache(const Def* def){
        if (auto load = is_load_val(def)) {
            if (!war_analysis.is_overwritten(def)){
                return false;
            }
        } else if (def->isa<Lit>() || isa_nested_var(def)) {
            return false;
        }

        return true;
    }

    bool depends_on_cache(const Def* def, DefSet& set, bool init = true){
        if(!init && set.contains(def)) return true;
        if(auto app = def->isa<App>()){
            return depends_on_cache(app->arg(), set, false);
        }else if( auto tuple = def->isa<Tuple>() ){
            for( auto op : tuple->ops() ){
                if(!depends_on_cache(op, set, false)){
                    return false;
                }
            }

            return true;
        }

        return false;
    }

    DefSet optimize(DefSet defs){
        DefSet filtered;
        for (auto def : defs) {
            if(needs_cache(def) && !depends_on_cache(def, defs)){
                filtered.insert(def);
            }  
        }


        canonicalize(filtered);

        DefSet result;
        for( DefSet& group : groups ){
            best = std::make_shared<CacheState>();
            for (auto def : group) {
                if(needs_cache(def)){
                    best->cached_defs.insert(def);

                    if(!requires_cache(def)){
                        best->queue.push(def);
                    }
                }


            }
            search(best);

            result.insert(best->cached_defs.begin(), best->cached_defs.end());
        }

        size_t size = result.size();
        
        return result;
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
    
};

} // namespace thorin::autodiff
