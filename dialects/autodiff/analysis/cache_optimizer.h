#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "affine_dfa.h"
#include "dialects/affine/affine.h"
#include "dialects/autodiff/analysis/affine_dfa.h"
#include "dialects/autodiff/analysis/alias_analysis.h"
#include "dialects/autodiff/analysis/analysis.h"
#include "dialects/autodiff/analysis/ptr_analysis.h"
#include "dialects/autodiff/analysis/utils.h"
#include "dialects/autodiff/analysis/war_analysis.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

struct CacheState {
    std::queue<const Def*> queue;
    size_t memory_estimate;
    size_t loop_factor;
    DefSet cached_defs;
    DefSet computed_defs;

    bool is_better_than(std::shared_ptr<CacheState>& other) {
        auto this_caches  = cached_defs.size();
        auto other_caches = other->cached_defs.size();
        if (this_caches < other_caches) return true;
        if (this_caches > other_caches) return false;
        return computed_defs.size() <= other->computed_defs.size();
    }
};

template<class T>
size_t hash_of_set(T set) {
    size_t hash = 0;

    for (auto def : set) { hash ^= GIDHash<const Def*>()(def); }

    return hash;
}

struct CacheStateHash {
    size_t operator()(std::shared_ptr<CacheState> p) const {
        return hash_of_set(p->cached_defs) ^ (hash_of_set(p->computed_defs) * 31);
    };
};

struct CacheStateEq {
    bool operator()(std::shared_ptr<CacheState> a, std::shared_ptr<CacheState> b) const {
        return a->cached_defs == b->cached_defs && a->computed_defs == b->computed_defs;
    }
};

class CacheOptimizer : public Analysis {
public:
    absl::flat_hash_set<std::shared_ptr<CacheState>, CacheStateHash, CacheStateEq> explored;
    std::shared_ptr<CacheState> best;
    AffineDFA& dfa;
    WarAnalysis& war;
    Utils& utils;
    AliasAnalysis& alias;
    std::vector<DefSet> groups;

    CacheOptimizer(AnalysisFactory& factory);
    void explore(std::shared_ptr<CacheState>& last, const Def* removed, const Def* parts);
    void find(const Def* def);
    bool is_flow_op(const Def* def);
    void canonicalize(DefSet defs);
    void find(AffineDFNode* node, DefSet& visited);
    void search(std::shared_ptr<CacheState> current);
    bool requires_cache(const Def* def);
    bool needs_cache(const Def* def);
    bool depends_on_cache(const Def* def, DefSet& set, bool init = true);
    DefSet optimize(DefSet defs);
};

} // namespace thorin::autodiff
