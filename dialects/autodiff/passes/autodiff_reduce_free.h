#pragma once

#include "thorin/rewrite.h"

#include "thorin/analyses/schedule.h"
#include "thorin/pass/pass.h"

namespace thorin::autodiff {

struct Test {
    const Lam* lam;
    DefVec extra;
};

class AutodiffRewriter {
public:
    AutodiffRewriter(World& w)
        : w(w) {}

    const Def* rewrite(const Def* old_def) {
        assert(old_def);
        if (old_def->isa<Univ>() || old_def->isa<Axiom>()) return old_def;
        if (auto i = old2new_.find(old_def); i != old2new_.end()) return i->second;
        auto new_def      = rewrite_convert(old_def);
        old2new_[old_def] = new_def;
        old2new_[new_def] = new_def;
        return new_def;
    }

    const Def* rewrite_convert(const Def* old_def) {
        if (old_def->isa<Var>()) {
            DefVec result;
            for (auto proj : old_def->projs()) {
                auto new_def = old2new_[proj];
                assert(new_def);
                result.push_back(new_def);
            }

            return w.tuple(result);
        } else if (auto extract = old_def->isa<Extract>()) {
            return w.extract(rewrite(extract->tuple()), rewrite(extract->index()));
        } else if (auto tuple = old_def->isa<Tuple>()) {
            DefArray array(tuple->ops(), [&](const Def* op) { return rewrite(op); });
            for (size_t i = 0; i < array.size(); i++) { array[i]->dump(1); }

            return w.tuple(array);
        } else if (auto pack = old_def->isa<Pack>()) {
            return w.pack(rewrite(pack->shape()), rewrite(pack->body()));
        } else if (old_def->isa<Lit>()) {
            return old_def;
        } else if (auto app = old_def->isa<App>()) {
            return w.app(rewrite(app->callee()), rewrite(app->arg()));
        } else if (auto extract = old_def->isa<Extract>()) {
            return w.extract(rewrite(extract->tuple()), rewrite(extract->index()));
        }

        return old_def;
    }

    void map(const Def* old_def, const Def* new_def) { old2new_[old_def] = new_def; }

private:
    World& w;
    Def2Def old2new_;
};

class AutodiffReduceFree : public RWPass<AutodiffReduceFree, Lam> {
public:
    AutodiffReduceFree(PassMan& man)
        : RWPass(man, "autodiff_reduce")
        , rewriter(world()) {}

    const Def* rewrite(const Def*) override;
    Lam* reduce(Lam* lam);
    void find(Lam* parent, const Def* def, DefSet& bin);
    void find(Lam* parent, Lam* child, DefSet& bin);
    void explore(Lam* lam);
    void map(const Def* old_def, const Def* new_def) { rewriter.map(old_def, new_def); }

    const Def* rew(const Def* def) { return rewriter.rewrite(def); }
    AutodiffRewriter rewriter;
    DefSet visited;
    Scheduler scheduler;
    std::unordered_map<Lam*, Test> tests;

    Def2Def extras_;
    Def2Def old2new_;
    DefSet visited_;
};

} // namespace thorin::autodiff
