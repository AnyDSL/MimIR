#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "affine_cfa.h"
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
    long memory;
    std::vector<size_t> depths;
    DefSet cached_defs;
    DefSet computed_defs;

    bool is_better_than(std::shared_ptr<CacheState>& other) {
        auto this_caches  = cached_defs.size();
        auto other_caches = other->cached_defs.size();
        auto cmp          = compare_depth(other);

        if (cmp == 1) return true;
        if (cmp == -1) return false;

        if (this_caches < other_caches) return true;
        if (this_caches > other_caches) return false;
        if (memory < other->memory) return true;
        if (memory > other->memory) return false;
        return computed_defs.size() <= other->computed_defs.size();
    }

    void add_depth(size_t depth) {
        depths.resize(depth + 1);
        depths[depth]++;
    }

    void remove_depth(size_t depth) { depths[depth]--; }

    int compare_depth(std::shared_ptr<CacheState>& other) {
        auto left_size  = depths.size();
        auto right_size = other->depths.size();
        auto max        = std::max(left_size, right_size);
        for (size_t i = 0; i < max; i++) {
            if (i < left_size && i < right_size) {
                auto left  = depths[i];
                auto right = other->depths[i];
                if (left < right) {
                    return 1;
                } else if (left > right) {
                    return -1;
                }
            } else if (i < left_size) {
                auto left = depths[i];
                if (left > 0) { return -1; }
            } else {
                auto right = other->depths[i];
                if (right > 0) { return 1; }
            }
        }

        return 0;
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
    AffineCFA& cfa;
    WarAnalysis& war;
    Utils& utils;
    AliasAnalysis& alias;
    std::vector<DefVec> groups;

    CacheOptimizer(AnalysisFactory& factory);
    size_t get_depth(const Def* op);
    size_t memory_estimate(const Def* ty);
    void explore(std::shared_ptr<CacheState>& last, const Def* removed, const Def* parts);
    void canonicalize(DefSet& defs);
    void search(std::shared_ptr<CacheState> current);
    bool needs_cache(const Def* def);
    bool depends_on_cache(const Def* def, DefSet& set, bool init = true);
    DefSet optimize(DefSet defs);
};

} // namespace thorin::autodiff
