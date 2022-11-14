#include "dialects/autodiff/passes/autodiff_reduce_free.h"

#include "thorin/analyses/schedule.h"

#include "dialects/affine/affine.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/math/math.h"

namespace thorin::autodiff {

void AutodiffReduceFree::find(Lam* parent, const Def* def, DefSet& bin) {
    if (def->isa<Lit>()) { return; }

    if (scheduler.smart(def) == parent) {
        bin.insert(def);
        return;
    } else if (auto app = def->isa<App>()) {
        auto arg = app->arg();
        for (auto op : arg->projs()) { find(parent, op, bin); }
    }
}

void AutodiffReduceFree::find(Lam* parent, Lam* child, DefSet& bin) {
    Scope scope(child);
    for (auto free_def : scope.free_defs()) {
        if (match<math::F>(free_def->type())) { find(parent, free_def, bin); }
    }
}

bool is_return(const Def* callee) {
    auto extract = callee->isa<Extract>();
    return extract && extract->tuple()->isa<Var>();
}

void for_each_child(const Def* callee, std::function<void(Lam*)> f) {
    if (auto callee_lam = callee->isa_nom<Lam>()) {
        f(callee_lam);
    } else if (auto extract = callee->isa<Extract>()) {
        for (auto branch : extract->tuple()->projs()) {
            auto branch_lam = branch->as_nom<Lam>();
            f(branch_lam);
        }
    }
}

void AutodiffReduceFree::explore(Lam* parent) {
    if (!visited_.insert(parent).second) return;
    auto& w   = world();
    auto body = parent->body();

    auto parent_ty = parent->type()->as<Pi>();

    auto parent_extras = extras_[parent];

    if (parent_extras) { parent_ty = w.cn(merge_sigma(parent_ty->dom(), parent_extras->type()->projs())); }

    auto new_parent = w.nom_lam(parent_ty, parent->dbg());
    new_parent->set_filter(false);
    new_parent->set_body(w.bot(w.type_nat()));
    // old2new_[parent] = new_parent;

    if (parent_extras) {
        auto old_num_vars = parent->num_vars();
        auto new_num_vars = new_parent->num_vars();
        for (size_t i = 0; i < new_num_vars; i++) {
            if (i < old_num_vars) {
                map(parent->var(i), new_parent->var(i));
            } else {
                map(parent_extras->proj(i - old_num_vars), new_parent->var(i));
            }
        }
    } else {
        map(parent->var(), new_parent->var());
    }

    map(parent, new_parent);

    if (auto app = body->isa<App>()) {
        auto arg    = app->arg();
        auto callee = app->callee();

        auto new_arg = rew(arg);

        // map(arg, new_arg);

        if (is_return(callee)) {
            new_parent->set_body(rew(app));
            return;
        }

        DefSet bin;
        for_each_child(callee, [&](Lam* child) { find(parent, child, bin); });

        DefVec extras_vec(bin.begin(), bin.end());
        const Def* extras = w.tuple(extras_vec);

        new_arg = merge_tuple(new_arg, rew(extras)->projs());

        for_each_child(callee, [&](Lam* child) {
            extras_[child] = extras;
            explore(child);
        });

        auto new_callee = rew(app->callee());
        app->dump(1);
        new_callee->dump(1);
        new_parent->set_body(w.app(new_callee, new_arg));

        /*if( auto callee_lam = callee->isa_nom<Lam>() ){
            auto new_callee = old2new_[callee_lam];
            new_parent->set_body(w.app(new_callee, new_arg));
        }else if( auto extract = callee->isa<Extract>() ){
            DefVec branches;
            for( auto branch : extract->tuple()->projs() ){
                auto branch_lam = branch->as_nom<Lam>();
                auto new_branch = old2new_[branch_lam];
                branches.push_back(new_branch);
            }

            auto branch_tup = w.tuple(branches);
            auto branch_extract = w.extract(branch_tup, extract->index());
            new_parent->set_body(w.app(branch_extract, new_arg));
        }*/

        //
        // tests.emplace(parent, Test{.lam = parent, .extra = extra});
    }
}

/*
void AutodiffReduceFree::explore(Lam* parent, Lam* child, DefSet& bin){

    find(parent, scope, bin);
    explore(child);
}*/
Lam* AutodiffReduceFree::reduce(Lam* lam) {
    Scope scope(lam);
    scheduler = Scheduler(scope);
    explore(lam);
    return rew(lam)->as_nom<Lam>();
}

const Def* AutodiffReduceFree::rewrite(const Def* def) {
    if (auto ad_app = match<ad>(def); ad_app && !visited.contains(def)) {
        auto diffee  = ad_app->arg()->as_nom<Lam>();
        auto reduced = reduce(diffee);
        def          = op_autodiff(reduced);
        visited.insert(def);
    }

    return def;
}

} // namespace thorin::autodiff
