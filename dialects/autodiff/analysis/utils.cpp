#include "thorin/def.h"

#include "dialects/autodiff/analysis/analysis_factory.h"
#include "dialects/autodiff/analysis/helper.h"
#include "dialects/mem/autogen.h"

namespace thorin::autodiff {

Utils::Utils(AnalysisFactory& factory)
    : Analysis(factory) {
    find_lams(factory.lam());
}

bool Utils::is_loop_index(const Def* def) {
    if (auto it = loop_indices_.find(def); it != loop_indices_.end()) return it->second;
    auto& alias = factory().alias();
    bool result = false;
    if (auto index_var = is_var(alias.get(def))) {
        auto loop_body = index_var->nom();
        auto& cfa      = factory().cfa();
        auto cfa_node  = cfa.node(loop_body);

        if (auto for_app_def = cfa_node->pred_or_null()) {
            if (auto for_app_lam = for_app_def->def()->isa_nom<Lam>()) {
                if (auto for_app = match<affine::For>(for_app_lam->body())) { result = for_app->arg(4) == loop_body; }
            }
        }
    }
    loop_indices_[def] = result;
    return result;
}

bool Utils::is_root_var(const Def* def) {
    if (auto var = is_var(def)) { return factory().lam() == var->nom(); }

    return false;
}

DefSet& Utils::depends_on_loads(const Def* def) {
    if (auto it = load_deps_.find(def); it != load_deps_.end()) { return *it->second; }

    def = factory().alias().get(def);

    load_deps_[def] = std::make_unique<DefSet>();
    DefSet& deps    = *load_deps_[def];
    if (auto lam = def->isa_nom<Lam>()) {
        auto& s = scope(lam);

        for (auto bound : factory().dfa().post_order(lam)) {
            DefSet& other_deps = depends_on_loads(bound->def());
            if (!other_deps.empty()) { bound->def()->dump(); }
            deps.insert(other_deps.begin(), other_deps.end());
        }
    } else if (match<mem::load>(def)) {
        deps.insert(def);
    } else {
        if (match<mem::M>(def->type()) || def->isa<Var>()) {
            return deps;
        } else if (auto app = def->isa<App>()) {
            DefSet& other_deps = depends_on_loads(arg(app));
            deps.insert(other_deps.begin(), other_deps.end());
        } else if (!def->isa<Var>()) {
            for (auto op : def->ops()) {
                DefSet& other_deps = depends_on_loads(op);
                deps.insert(other_deps.begin(), other_deps.end());
            }
        }
    }

    return deps;
}

} // namespace thorin::autodiff
