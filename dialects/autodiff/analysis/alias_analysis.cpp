#include "dialects/autodiff/analysis/alias_analysis.h"

#include "thorin/def.h"

#include "dialects/autodiff/analysis/analysis_factory.h"
#include "dialects/mem/autogen.h"

namespace thorin::autodiff {
AliasAnalysis::AliasNode& AliasAnalysis::alias_node(const Def* def) {
    auto it = alias_nodes.find(def);
    if (it == alias_nodes.end()) { it = alias_nodes.emplace(def, std::make_unique<AliasNode>(def)).first; }
    return *it->second;
}

void AliasAnalysis::run() {
    auto& lams = factory().utils().lams();

    for (auto lam : lams) {
        auto app = lam->as_nom<Lam>()->body()->as<App>();
        meet_app(app);
    }

    todo_ = true;
    for (; todo_;) {
        todo_ = false;
        for (auto& [def, alias_node] : alias_nodes) {
            auto& alias_set = alias_node->alias_set();
            AliasNodes new_nodes;
            for (auto node : alias_set) {
                auto prev = node->alias();
                if (prev != nullptr) {
                    todo_ = true;
                    new_nodes.insert(prev);
                } else {
                    new_nodes.insert(node);
                }
            }

            alias_node->set(new_nodes);
        }
    }
}

void AliasAnalysis::meet(const Def* def, const Def* ref) {
    auto& def_lat = alias_node(def);
    alias_node(ref).add(&def_lat);
}

void AliasAnalysis::meet_app(const App* app) {
    if (match<affine::For>(app)) {
        auto acc  = app->arg(3);
        auto loop = app->arg(4)->as_nom<Lam>();
        auto exit = app->arg(5)->isa_nom<Lam>();

        for (auto preds : factory().cfa().preds(loop->ret_var())) {
            auto lam  = preds->def()->as_nom<Lam>();
            auto body = lam->body()->as<App>();
            auto arg  = body->arg();

            meet_projs(arg, loop->var(1));
            if (exit) meet_projs(arg, exit->var());
        }

        if (exit) meet_projs(acc, exit->var());
        meet_projs(acc, loop->var(1));
    } else {
        auto callee = app->callee();
        auto arg    = app->arg();
        meet_app(callee, arg);
    }
}

void AliasAnalysis::meet_projs(const Def* def, const Def* ref) {
    assert(def->type() == ref->type());
    for (size_t i = 0; i < def->num_projs(); i++) { meet(def->proj(i), ref->proj(i)); }
}

void AliasAnalysis::meet_app(const Def* callee, const Def* arg) {
    if (auto callee_lam = callee->isa_nom<Lam>()) {
        meet_projs(arg, callee_lam->var());
    } else if (auto extract = is_branch(callee)) {
        for (auto branch : extract->tuple()->ops()) { meet_app(branch, arg); }
    } else {
        callee->dump();
    }
}
} // namespace thorin::autodiff
