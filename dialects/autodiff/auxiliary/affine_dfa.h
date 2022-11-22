#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/autodiff/auxiliary/autodiff_flow_analysis.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

class AnalysisFactory;
struct AffineDFNode;
using AffineDFNodes = std::unordered_set<AffineDFNode*>;

struct AffineDFNode {
    AffineDFNode(const Def* def)
        : def_(def) {}

    const Def* def_;
    AffineDFNodes preds_;
    AffineDFNodes succs_;
    size_t visited_ = 0;
    bool flag_      = true;

    const Def* def() { return def_; }

    AffineDFNodes& preds() { return preds_; }
    AffineDFNodes& succs() { return succs_; }

    AffineDFNode* pred() {
        assert(preds_.size() == 1);
        return *preds_.begin();
    }
};

class AffineDFA : public Analysis {
public:
    DefMap<std::unique_ptr<AffineDFNode>> nodes_;
    DefMap<std::vector<AffineDFNode*>> post_order_map_;
    std::vector<AffineDFNode*>* post_order_;
    DefSet ops_;
    size_t visited_counter_ = 0;
    const Def* arg          = nullptr;

    AffineDFA(AnalysisFactory& factory);
    AffineDFA(AffineDFA& other) = delete;

    DefSet& ops() { return ops_; }

    AffineDFNodes& succs(const Def* def) {
        auto node = create(def);
        return node->succs();
    }

    AffineDFNode* node(const Def* def) { return create(def, false); }

    AffineDFNode* create(const Def* def, bool init = true) {
        auto it = nodes_.find(def);
        if (it == nodes_.end()) {
            if (!init) return nullptr;
            it = nodes_.emplace(def, std::make_unique<AffineDFNode>(def)).first;
        }
        return it->second.get();
    }

    size_t size() { return nodes_.size(); }

    std::vector<AffineDFNode*>& post_order(Lam* lam) { return post_order_map_[lam]; }

private:
    bool link(AffineDFNode* left, AffineDFNode* right) {
        auto size_before = left->succs_.size();

        left->succs_.insert(right);
        right->preds_.insert(left);

        return size_before != left->succs_.size();
    }

    void run();

    void run(Lam* lam) {
        auto app = lam->body()->as<App>();
        if (auto loop = match<affine::For>(app)) {
            arg = loop->arg(3);
        } else {
            arg = app->arg();
        }

        run(arg);

        auto var_node = node(lam->var());
        assert(var_node);
        visited_counter_++;
        post_order_ = &post_order_map_[lam];
        post_order_visit(var_node);
    }

    void run(const Def* def) {
        if (def->isa_nom<Lam>()) return;
        if (!ops_.insert(def).second) return;
        auto def_node = create(def);
        for (auto op : def->ops()) {
            link(create(op), def_node);
            run(op);
        }
    }

    void post_order_visit(AffineDFNode* n) {
        if (n->visited_ == visited_counter_) return;
        n->visited_ = visited_counter_;

        if (n->def() == arg) {
            post_order_->push_back(n);
            n->flag_ = true;
        } else {
            bool flag = false;
            for (auto succ : n->succs()) {
                post_order_visit(succ);
                flag |= succ->flag_;
            }

            if (flag) { post_order_->push_back(n); }
            n->flag_ = flag;
        }
    }
};

} // namespace thorin::autodiff
