#pragma once

#include "thorin/rewrite.h"

#include "thorin/analyses/schedule.h"
#include "thorin/pass/pass.h"

#include "dialects/mem/mem.h"

namespace thorin::autodiff {

struct Old2New {
    std::unique_ptr<Old2New> parent_ = nullptr;
    Def2Def map_;

    const Def* has(const Def* old_def) {
        if (auto i = map_.find(old_def); i != map_.end()) { return i->second; }
        if (parent_ != nullptr) return parent_->has(old_def);
        return nullptr;
    }

    void map(const Def* old_def, const Def* new_def) {
        map_[old_def] = new_def;
        map_[new_def] = new_def;
    }

    void map_global(const Def* old_def, const Def* new_def) {
        if (parent_ != nullptr) {
            parent_->map_global(old_def, new_def);
        } else {
            map(old_def, new_def);
        }
    }
};

class InferRewriter {
public:
    InferRewriter(World& w)
        : w(w)
        , old2new_(std::make_unique<Old2New>()) {}

    const Def* rewrite(const Def* old_def) {
        assert(old_def);
        if (old_def->isa<Univ>() || old_def->isa<Axiom>()) return old_def;
        const Def* new_def = has(old_def);
        if (!new_def) {
            new_def = rewrite_convert(old_def);
            map(old_def, new_def);
        }

        assert(new_def);
        return new_def;
    }

    const Def* check_projs(const Def* old_def) {
        if (old_def->sort() == Sort::Term && old_def->num_projs() > 1) {
            DefVec vec;
            for (auto proj : old_def->projs()) {
                const Def* new_def;
                if (auto extract = proj->isa<Extract>()) {
                    new_def = has(extract);
                } else {
                    new_def = rewrite(proj);
                }

                if (!new_def) { return nullptr; }
                vec.push_back(new_def);
            }
            return w.tuple(vec, old_def->dbg());
        }
        return nullptr;
    }

    const Def* rewrite_convert(const Def* old_def) {
        if (auto new_def = check_projs(old_def)) { return new_def; }

        if (old_def->isa<Var>()) {
            if (old_def->type() == w.sigma()) { return w.tuple(); }
            assert(false);
        } else if (auto tuple = old_def->isa<Tuple>()) {
            DefArray array(tuple->ops(), [&](const Def* op) { return rewrite(op); });
            return w.tuple(array, old_def->dbg());
        } else if (auto pack = old_def->isa<Pack>()) {
            return w.pack(rewrite(pack->shape()), rewrite(pack->body()), old_def->dbg());
        } else if (old_def->isa<Lit>()) {
            return old_def;
        } else if (auto app = old_def->isa<App>()) {
            return w.app(rewrite(app->callee()), rewrite(app->arg()), old_def->dbg());
        } else if (auto extract = old_def->isa<Extract>()) {
            return w.extract(rewrite(extract->tuple()), rewrite(extract->index()), old_def->dbg());
        }

        assert(!old_def->isa_nom<Lam>());

        auto new_ops = DefArray(old_def->ops(), [&](const Def* op) { return rewrite(op); });
        auto new_def = old_def->rebuild(w, old_def->type(), new_ops, old_def->dbg());
        return new_def;
    }

    void map(const Def* old_def, const Def* new_def) {
        new2old_[new_def] = old_def;
        old2new_->map(old_def, new_def);
    }

    void map_global(const Def* old_def, const Def* new_def) {
        new2old_[new_def] = old_def;
        old2new_->map_global(old_def, new_def);
    }

    const Def* has(const Def* def) { old2new_->has(def); }

    const Def* new2old(const Def* def) { return new2old_[def]; }

    void push() {
        auto new_old2new     = std::make_unique<Old2New>();
        new_old2new->parent_ = std::move(old2new_);
        old2new_             = std::move(new_old2new);
    }

    void pop() { old2new_ = std::move(old2new_->parent_); }

private:
    World& w;
    Def2Def new2old_;
    std::unique_ptr<Old2New> old2new_;
};

using filter_t = std::function<bool(const Def*)>;

class DefInliner {
public:
    DefInliner(Lam* lam)
        : lam_(lam)
        , w_(lam->world())
        , scope(lam)
        , scheduler(scope)
        , r_(lam->world()) {}

    Lam* build();
    DefVec get_extra(Lam* lam);
    DefVec get_extra_ty(Lam* lam);
    Lam* build(Lam* parent, Lam* lam);
    void map_free_to_var(Lam* parent, Lam* old_lam, Lam* new_lam);
    void add_extra(Lam* dst_lam, const Def* def);

    const Def* rew(const Def* old_def) { return r_.rewrite(old_def); }
    void map(const Def* old_def, const Def* new_def) { r_.map(old_def, new_def); }
    void map_global(const Def* old_def, const Def* new_def) { r_.map_global(old_def, new_def); }
    const Def* has(const Def* old_def) { return r_.has(old_def); }
    Lam* find_lam(const Def* def);
    void meet(const Def* src, const Def* dst);
    void propagate(const CFNode* node);

    bool validate(const Def* def);

    template<class Id>
    void allow() {
        filter([](const Def* def) { return match<Id>(def->type()); });
    }

    template<class Id>
    void forbid() {
        filter([](const Def* def) { return !match<Id>(def->type()); });
    }

    void filter(filter_t filter) { filters.push_back(filter); }

    void push() { r_.push(); }
    void pop() { r_.pop(); }

    Scope scope;
    Scheduler scheduler;

    NomMap<DefSet> nom2defs;
    DefMap<Def*> def2nom;
    NomSet lams;
    DefMap<DefSet> extras;
    DefMap<DefSet> extras_fwd;
    DefMap<DefVec> extras_sorted;

    InferRewriter r_;
    World& w_;
    Lam* lam_;
    bool todo_;

    std::vector<filter_t> filters;
};

class PropifyPass : public RWPass<PropifyPass, Lam> {
public:
    PropifyPass(PassMan& man)
        : RWPass(man, "autodiff_reduce") {}

    const Def* rewrite(const Def*) override;
    DefSet visited_;
};

} // namespace thorin::autodiff
