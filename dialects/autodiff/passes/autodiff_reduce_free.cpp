#include "dialects/autodiff/passes/autodiff_reduce_free.h"

#include "thorin/analyses/schedule.h"

#include "dialects/affine/affine.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

bool is_return(const Def* callee) {
    auto extract = callee->isa<Extract>();
    return extract && extract->tuple()->isa<Var>();
}

void for_each_lam(const Def* callee, std::function<void(Lam*)> f) {
    if (is_return(callee)) return;
    if (auto callee_lam = callee->isa_nom<Lam>()) {
        f(callee_lam);
    } else if (auto extract = callee->isa<Extract>()) {
        for (auto branch : extract->tuple()->projs()) { for_each_lam(branch, f); }
    }
}

DefVec AutodiffReduceFree::get_extra(Lam* lam) {
    DefVec result;
    if (lam) { result = extras_[lam]; }

    return result;
}

DefVec AutodiffReduceFree::get_extra_ty(Lam* lam) {
    DefVec extra = get_extra(lam);
    DefVec result;

    for (auto def : extra) { result.push_back(def->type()); }

    return result;
}

void AutodiffReduceFree::map_free_to_var(Lam* parent, Lam* old_lam, Lam* new_lam) {
    auto extra = get_extra(parent);
    auto dom   = old_lam->type()->as<Pi>()->dom();
    if (dom->num_projs() == 0 && extra.size() == 1) {
        map(extra[0], new_lam->var());
    } else {
        auto old_num_vars = old_lam->num_vars();
        auto new_num_vars = new_lam->num_vars();
        auto vars         = new_lam->vars();
        for (size_t i = 0; i < new_num_vars; i++) {
            if (i < old_num_vars) {
                map(old_lam->var(i), vars[i]);
            } else {
                map(extra[i - old_num_vars], vars[i]);
            }
        }
    }
}

Lam* AutodiffReduceFree::build(Lam* parent, Lam* lam) {
    if (auto new_lam = has(lam)) return new_lam->as_nom<Lam>();

    auto& w       = world();
    auto extra_ty = get_extra_ty(parent);

    auto dom = lam->type()->as<Pi>()->dom();

    auto lam_ty = w.cn(merge_sigma(dom, extra_ty));

    auto new_lam = w.nom_lam(lam_ty, lam->dbg());
    map(lam, new_lam);

    if (parent == nullptr) { map(lam->ret_var(), new_lam->ret_var()); }

    new_lam->set_filter(false);
    new_lam->set_body(w.bot(w.type_nat()));

    auto body = lam->body();
    if (auto app = body->isa<App>()) {
        auto arg    = app->arg();
        auto callee = app->callee();

        save();
        map_free_to_var(parent, lam, new_lam);
        auto new_arg = merge_tuple(rew(arg), rew(w.tuple(get_extra(lam)))->projs());
        restore();

        for_each_lam(callee, [&](Lam* child) {
            map_free_to_var(parent, lam, new_lam);
            build(lam, child);
        });

        map_free_to_var(parent, lam, new_lam);
        new_lam->set_body(w.app(rew(app->callee()), new_arg));
    }

    return new_lam;
}

void mem_first_sort(DefVec& vec) {
    std::sort(vec.begin(), vec.end(), [](const Def* left, const Def* right) { return !match<mem::M>(right->type()); });
}

void AutodiffReduceFree::prop_extra(const F_CFG& cfg,
                                    DefMap<DefSet>& extras,
                                    const CFNodes& nodes,
                                    const Def* def,
                                    Def* nom) {
    for (auto node : nodes) {
        auto current = node->nom();

        for (auto op : def->projs()) {
            if (!match<mem::Ptr>(op->type())) { extras[current].insert(op); }
        }

        if (current != nom) { prop_extra(cfg, extras, cfg.preds(current), def, nom); }
    }
}

Lam* AutodiffReduceFree::reduce(Lam* lam) {
    if (auto new_lam = has(lam)) return new_lam->as_nom<Lam>();

    Scope scope(lam);
    scheduler = Scheduler(scope);

    NomMap<DefSet> nom2defs;
    DefMap<Def*> def2nom;
    NomSet lams;

    for (auto bound : scope.bound()) {
        if (!bound->isa<Lit>() && !bound->isa<Var>() && !bound->type()->isa<Pi>()) {
            auto lam = scheduler.smart(bound);
            nom2defs[lam].insert(bound);
            def2nom[bound] = lam;
            lams.insert(lam);
        }
    }

    auto& cfg = scope.f_cfg();

    DefMap<DefSet> extras;
    for (auto dst_lam : lams) {
        for (auto def : nom2defs[dst_lam]) {
            for (auto op : def->ops()) {
                auto src_lam = def2nom[op];
                if (src_lam && src_lam != dst_lam) { prop_extra(cfg, extras, cfg.preds(dst_lam), op, src_lam); }
            }
        }
    }

    for (auto [lam, set] : extras) {
        DefVec extra(set.begin(), set.end());
        mem_first_sort(extra);
        extras_.emplace(lam, extra);
    }

    auto result = build(nullptr, lam);

    return result;
}

const Def* AutodiffReduceFree::rewrite(const Def* def) {
    if (auto ad_app = match<ad>(def)) {
        auto diffee = ad_app->arg()->as_nom<Lam>();
        def         = op_autodiff(reduce(diffee));
    }

    return def;
}

} // namespace thorin::autodiff
