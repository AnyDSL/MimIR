#include "dialects/autodiff/analysis/cache_optimizer.h"

#include "thorin/def.h"
#include "thorin/tuple.h"

#include "dialects/autodiff/analysis/analysis.h"
#include "dialects/autodiff/analysis/analysis_factory.h"
#include "dialects/autodiff/utils/union.h"
#include "dialects/core/core.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

CacheOptimizer::CacheOptimizer(AnalysisFactory& factory)
    : Analysis(factory)
    , war(factory.war())
    , cfa(factory.cfa())
    , dfa(factory.dfa())
    , utils(factory.utils())
    , alias(factory.alias())
    , best(nullptr) {}

size_t CacheOptimizer::get_depth(const Def* op) {
    Lam* lam  = utils.lam_of_op(op);
    auto node = cfa.node(lam);
    return node->depth();
}

size_t CacheOptimizer::memory_estimate(const Def* ty) {
    auto before_memory = core::op(core::trait::size, ty);

    if (auto lit = isa_lit(before_memory)) { return *lit; }

    return 8;
}

void CacheOptimizer::explore(std::shared_ptr<CacheState>& last, const Def* removed, const Def* parts) {
    auto next_state = std::make_shared<CacheState>(*last);

    next_state->memory -= memory_estimate(removed->type());
    size_t depth = get_depth(removed);

    auto test = next_state->depths.size();
    next_state->remove_depth(depth);

    next_state->cached_defs.erase(removed);
    if (next_state->computed_defs.insert(removed).second) {
        for (auto def : parts->projs()) {
            def = alias.get(def);
            if (!next_state->computed_defs.contains(def)) {
                if (needs_cache(def) && !depends_on_cache(def, next_state->cached_defs)) {
                    auto result = next_state->cached_defs.insert(def);

                    if (result.second) {
                        size_t depth = get_depth(def);
                        next_state->add_depth(depth);
                        next_state->memory += memory_estimate(def->type());
                        next_state->queue.push(def);
                    }
                }
            }
        }
    }

    if (explored.insert(next_state).second) {
        if (next_state->is_better_than(best)) { best = next_state; }

        search(next_state);
    }
}

class Canonicalize {
public:
    Canonicalize(AnalysisFactory& factory)
        : alias(factory.alias()) {}
    AliasAnalysis& alias;
    DefSet visited;
    std::unordered_map<const Def*, std::unique_ptr<DefUnionNode>> unions;

    UnionNode<const Def*>* get(const Def* def) {
        auto i = unions.find(def);
        if (i == unions.end()) {
            auto p = unions.emplace(def, std::make_unique<DefUnionNode>(def));
            assert_unused(p.second);
            i = p.first;
        }
        return &*i->second;
    }

    void groups(std::vector<DefVec>& result) {
        std::unordered_map<DefUnionNode*, DefVec> map;

        for (auto& [def, node] : unions) {
            if (visited.contains(def)) {
                auto repr = find(node.get());
                map[repr].push_back(def);
            }
        }

        result.clear();
        for (auto& [node, defs] : map) { result.push_back(std::move(defs)); }
    }

    void run(const Def* def) {
        def = alias.get(def);
        visited.insert(def);
        propagate(def, get(def));
    }

    void propagate(const Def* def, DefUnionNode* node) {
        def = alias.get(def);
        unify(node, get(def));
        if (is_load_val(def)) return;
        if (auto app = def->isa<App>()) {
            propagate(app->arg(), node);
        } else if (def->num_projs() > 1) {
            for (auto proj : def->projs()) { propagate(proj, node); }
        } else {
            def->dump();
        }
    }
};

void CacheOptimizer::canonicalize(DefSet& defs) {
    Canonicalize can(factory());

    for (auto def : defs) { can.run(def); }

    can.groups(groups);
}

void CacheOptimizer::search(std::shared_ptr<CacheState> current) {
    auto& queue = current->queue;
    for (; queue.size() > 0;) {
        auto def = queue.front();
        queue.pop();
        if (auto app = def->isa<App>(); app && mem::mem_def(def) == nullptr) { explore(current, def, app->arg()); }
    }
}

bool CacheOptimizer::needs_cache(const Def* def) {
    if (factory().utils().is_loop_index(def)) return false;
    if (def->isa<Lit>()) return false;
    if (utils.is_root_var(def)) return false;
    if (is_load_val(def) && !war.is_overwritten(def)) return false;
    return true;
}

bool CacheOptimizer::depends_on_cache(const Def* def, DefSet& set, bool init) {
    def = alias.get(def);

    if (!needs_cache(def)) return true;
    if (!init && set.contains(def)) return true;
    if (auto app = def->isa<App>()) {
        return depends_on_cache(app->arg(), set, false);
    } else if (auto tuple = def->isa<Tuple>()) {
        for (auto op : tuple->ops()) {
            if (!depends_on_cache(op, set, false)) { return false; }
        }

        return true;
    } else if (auto pack = def->isa<Pack>()) {
        return depends_on_cache(pack->body(), set, false);
    }

    return false;
}

DefSet CacheOptimizer::optimize(DefSet defs) {
    DefSet required_caches;
    for (auto def : defs) {
        auto target = alias.get(def);
        if (needs_cache(target)) { required_caches.insert(target); }
    }

    DefSet no_deps;
    for (auto def : required_caches) {
        if (depends_on_cache(def, required_caches)) { continue; }
        no_deps.insert(def);
    }

    // return no_deps;

    canonicalize(no_deps);

    DefSet result;
    for (auto& group : groups) {
        explored.clear();
        best = std::make_shared<CacheState>();
        for (auto def : group) {
            def->dump(1);
            best->memory += memory_estimate(def->type());

            size_t depth = get_depth(def);
            best->add_depth(depth);

            best->cached_defs.insert(def);
            best->queue.push(def);
        }

        search(best);
        result.insert(best->cached_defs.begin(), best->cached_defs.end());
    }

    return result;
}

} // namespace thorin::autodiff
