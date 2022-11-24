#include "dialects/autodiff/analysis/live_analysis.h"

#include "thorin/def.h"

#include "dialects/autodiff/analysis/affine_cfa.h"
#include "dialects/autodiff/analysis/analysis.h"
#include "dialects/autodiff/analysis/analysis_factory.h"
#include "dialects/autodiff/utils/helper.h"

namespace thorin::autodiff {

LiveAnalysis::LiveAnalysis(AnalysisFactory& factory)
    : Analysis(factory) {
    run();
}

void LiveAnalysis::run() {}

Lam* lam_of_op(const Def* mem) {
    auto op = unextract(mem);
    if (auto var = op->isa<Var>()) {
        return var->nom()->as_nom<Lam>();
    } else {
        auto app = op->isa<App>();
        assert(app);

        return lam_of_op(app->arg(0));
    }
}

Lam* find_join_lam(AffineCFNode* node, LamSet& lams, DefMap<Lam*>& visited) {
    auto caller = node->def();
    if (auto it = visited.find(caller); it != visited.end()) return it->second;
    bool has_erased = false;
    if (auto lam = caller->isa_nom<Lam>()) {
        if (lams.erase(lam)) { has_erased = true; }
    }

    Lam* pred_lam = nullptr;
    size_t size   = 0;
    for (auto pred : node->preds()) {
        auto pred_lam_tmp = find_join_lam(pred, lams, visited);
        if (pred_lam_tmp && pred_lam_tmp != pred_lam) {
            pred_lam = pred_lam_tmp;
            size++;
        }
    }

    Lam* result;
    if (has_erased || size > 1) {
        result = caller->as_nom<Lam>();
    } else {
        result = pred_lam;
    }
    visited[caller] = result;
    return result;
}

Lam* find_join_lam(AffineCFNode* node, LamSet& lams) {
    DefMap<Lam*> visited;
    find_join_lam(node, lams, visited);
}

void LiveAnalysis::build_end_of_live() {
    end_of_live_ = std::make_unique<DefMap<Lam*>>();
    auto& utils  = factory().utils();
    auto& cfa    = factory().cfa();
    auto root    = factory().lam();
    auto exit    = cfa.node(root->ret_var());

    DefMap<LamSet> load2lams;
    for (auto lam : utils.lams()) {
        auto& result = utils.depends_on_loads(lam);

        for (auto def : result) { load2lams[def].insert(lam); }
    }

    for (auto& [load, lams] : load2lams) { (*end_of_live_)[load] = find_join_lam(exit, lams); }
}

Lam* LiveAnalysis::end_of_live(const Def* load) {
    assert(load);
    return end_of_live()[load];
}

DefMap<Lam*>& LiveAnalysis::end_of_live() {
    if (end_of_live_ == nullptr) { build_end_of_live(); }
    return *end_of_live_;
}

} // namespace thorin::autodiff
