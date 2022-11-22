#pragma once

#include <thorin/analyses/scope.h>
#include <thorin/axiom.h>
#include <thorin/def.h>
#include <thorin/lam.h>

#include "dialects/affine/affine.h"
#include "dialects/autodiff/analysis/analysis.h"
#include "dialects/autodiff/utils/helper.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

class AnalysisFactory;

class AliasAnalysis : public Analysis {
protected:
    class AliasNode;
    using AliasNodes = std::unordered_set<AliasNode*>;
    struct AliasNode {
        enum Type { Bot, Alias, Top };

        AliasNode(const Def* def)
            : type_(Type::Bot)
            , def_(def) {}

        Type type_;
        const Def* def_;
        AliasNodes alias_;

        const Def* def() { return def_; }

        AliasNode* alias() {
            if (alias_.size() == 1) { return *alias_.begin(); }

            return nullptr;
        }
        AliasNodes& alias_set() { return alias_; }
        Type type() { return type_; }

        void add(AliasNode* alias) {
            if (alias == this) return;
            alias_.insert(alias);
        }

        void set(AliasNode* alias) {
            alias_.clear();
            add(alias);
        }

        void set(AliasNodes& nodes) {
            alias_.clear();
            for (auto node : nodes) { add(node); }
        }
    };

public:
    DefMap<std::unique_ptr<AliasNode>> alias_nodes;
    NomSet lams;

    bool todo_                          = false;
    AliasAnalysis(AliasAnalysis& other) = delete;
    explicit AliasAnalysis(AnalysisFactory& factory)
        : Analysis(factory) {
        run();
    };

    AliasNode& alias_node(const Def* def);

    void meet(const Def* def, const Def* ref);

    void meet_app(const App* app);

    void meet_projs(const Def* def, const Def* ref);
    void meet_app(const Def* callee, const Def* arg);

    const Def* get(const Def* def) {
        auto alias = alias_node(def).alias();
        if (alias) { return alias->def(); }
        return def;
    }

    void run();
};

} // namespace thorin::autodiff
