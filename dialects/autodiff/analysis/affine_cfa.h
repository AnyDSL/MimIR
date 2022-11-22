#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/analysis/flow_analysis.h"
#include "dialects/autodiff/analysis/helper.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

class AnalysisFactory;
struct AffineCFNode;
using AffineCFNodes = std::unordered_set<AffineCFNode*>;

struct AffineCFNode {
    AffineCFNode(const Def* def)
        : def_(def) {}

    const Def* def_;
    AffineCFNodes preds_;
    AffineCFNodes succs_;
    bool visited_ = false;

    const Def* def() { return def_; }

    AffineCFNodes& preds() { return preds_; }
    AffineCFNodes& succs() { return succs_; }

    AffineCFNode* pred() {
        assert(preds_.size() == 1);
        return *preds_.begin();
    }

    AffineCFNode* pred_or_null() {
        if (preds_.size() != 1) return nullptr;
        return *preds_.begin();
    }
};

class AffineCFA : public Analysis {
public:
    DefMap<std::unique_ptr<AffineCFNode>> nodes_;
    DefSet visited_;
    std::vector<AffineCFNode*> post_order_;
    bool post_order_init_ = false;

    AffineCFA(AnalysisFactory& factory);
    AffineCFA(AffineCFA& other) = delete;

    AffineCFNodes& preds(const Def* def) {
        auto node = create(def);
        return node->preds();
    }

    AffineCFNode* node(const Def* def) { return create(def, false); }

    AffineCFNode* create(const Def* def, bool init = true) {
        auto it = nodes_.find(def);
        if (it == nodes_.end()) {
            if (!init) return nullptr;
            it = nodes_.emplace(def, std::make_unique<AffineCFNode>(def)).first;
        }
        return it->second.get();
    }

    size_t size() { return nodes_.size(); }

    std::vector<AffineCFNode*>& post_order() {
        if (!post_order_init_) {
            post_order_init_ = true;
            for (auto& [lam, node] : nodes_) { post_order_visit(node.get()); }
        }

        return post_order_;
    }

private:
    bool link(AffineCFNode* left, AffineCFNode* right) {
        auto size_before = left->succs_.size();

        left->succs_.insert(right);
        right->preds_.insert(left);

        return size_before != left->succs_.size();
    }

    void run(Lam* lam) {
        auto app = lam->body()->as<App>();
        if (auto loop = match<affine::For>(app)) {
            auto body = loop->arg(4)->as_nom<Lam>();
            auto exit = loop->arg(5);
            run_call(lam, body);
            run_call(lam, exit);
            run_call(body->ret_var(), exit);
        } else {
            run_call(lam, app->callee());
        }
    }

    void run(const Def* caller, const Def* callee) {
        link(create(caller), create(callee));
        if (!visited_.insert(callee).second) return;
        if (auto callee_lam = callee->isa_nom<Lam>()) { run(callee_lam); }
    }

    void run_call(const Def* caller, const Def* callee) {
        if (auto extract = is_branch(callee)) {
            for (auto branch : extract->tuple()->ops()) { run_call(caller, branch); }
        } else {
            run(caller, callee);
        }
    }

    void post_order_visit(AffineCFNode* n) {
        if (n->visited_) return;
        n->visited_ = true;

        for (auto succ : n->succs()) { post_order_visit(succ); }

        post_order_.push_back(n);
    }
};

} // namespace thorin::autodiff
