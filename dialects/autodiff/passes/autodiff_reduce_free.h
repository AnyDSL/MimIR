#pragma once

#include "thorin/rewrite.h"

#include "thorin/analyses/schedule.h"
#include "thorin/pass/pass.h"

namespace thorin::autodiff {

class FreeAvoidRewriter {
public:
    FreeAvoidRewriter(World& w)
        : w(w) {}

    const Def* rewrite(const Def* old_def) {
        assert(old_def);
        if (old_def->isa<Univ>() || old_def->isa<Axiom>()) return old_def;
        const Def* new_def;
        if (auto i = old2new_.find(old_def); i != old2new_.end()) {
            new_def = i->second;
        } else {
            new_def           = rewrite_convert(old_def);
            old2new_[old_def] = new_def;
            old2new_[new_def] = new_def;
        }

        assert(new_def);
        return new_def;
    }

    const Def* check_projs(const Def* old_def) {
        if (old_def->num_projs() > 1) {
            DefVec vec;
            for (auto proj : old_def->projs()) {
                const Def* new_def = nullptr;
                if (auto extract = proj->isa<Extract>()) {
                    if (auto i = old2new_.find(extract); i != old2new_.end()) { new_def = i->second; }
                } else {
                    new_def = rewrite(proj);
                }

                if (!new_def) { return nullptr; }
                vec.push_back(new_def);
            }
            return w.tuple(vec);
        }
        return nullptr;
    }

    const Def* rewrite_convert(const Def* old_def) {
        if (auto new_def = check_projs(old_def)) { return new_def; }

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

    void map(const Def* old_def, const Def* new_def) {
        old2new_[old_def] = new_def;
        old2new_[new_def] = new_def;
    }

    void map(Def2Def& old2new) { old2new_.insert(old2new.begin(), old2new.end()); }

    const Def* has(const Def* def) {
        if (auto i = old2new_.find(def); i != old2new_.end()) { return i->second; }
        return nullptr;
    }

    void save() { save_ = old2new_; }

    void restore() { old2new_ = save_; }

private:
    World& w;
    Def2Def old2new_;
    Def2Def save_;
};

class AutodiffReduceFree : public RWPass<AutodiffReduceFree, Lam> {
public:
    AutodiffReduceFree(PassMan& man)
        : RWPass(man, "autodiff_reduce")
        , r_(world()) {}

    const Def* rewrite(const Def*) override;
    Lam* reduce(Lam* lam);
    DefVec get_extra(Lam* lam);
    DefVec get_extra_ty(Lam* lam);
    Lam* build(Lam* parent, Lam* lam);
    void map_free_to_var(Lam* parent, Lam* old_lam, Lam* new_lam);
    void prop_extra(const F_CFG& cfg, DefMap<DefSet>& extras, const CFNodes& node, const Def* def, Def* lam);

    const Def* rew(const Def* old_def) { return r_.rewrite(old_def); }
    void map(const Def* old_def, const Def* new_def) { r_.map(old_def, new_def); }
    const Def* has(const Def* old_def) { return r_.has(old_def); }

    void save() { r_.save(); }

    void restore() { r_.restore(); }

    FreeAvoidRewriter r_;
    Scheduler scheduler;
    // Def2Def old2new_;
    DefMap<DefVec> extras_;
    const Def* new_return;
};

} // namespace thorin::autodiff
