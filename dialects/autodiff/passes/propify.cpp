#include "dialects/autodiff/passes/propify.h"

#include "thorin/analyses/schedule.h"

#include "dialects/affine/affine.h"
#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/auxiliary/autodiff_aux.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

static bool is_return(const Def* callee) {
    auto extract = callee->isa<Extract>();
    return extract && extract->tuple()->isa<Var>();
}

DefVec Propify::get_extra(Lam* lam) {
    DefVec result;
    if (lam) { result = extras_sorted[lam]; }

    return result;
}

DefVec Propify::get_extra_ty(Lam* lam) {
    DefVec extra = get_extra(lam);
    DefVec result;

    for (auto def : extra) { result.push_back(def->type()); }

    return result;
}

void Propify::map_free_to_var(Lam* parent, Lam* old_lam, Lam* new_lam) {
    auto extra = get_extra(parent);
    auto dom   = old_lam->type()->as<Pi>()->dom();
    if (dom->num_projs() == 0 && extra.size() == 1) {
        map(extra[0], new_lam->var());
    } else {
        const Def* old_ret = old_lam->ret_var();

        auto offset       = old_ret ? 1 : 0;
        auto old_num_vars = old_lam->num_vars() - offset;
        auto new_num_vars = new_lam->num_vars() - offset;
        auto vars         = new_lam->vars();
        for (size_t i = 0; i < new_num_vars; i++) {
            if (i < old_num_vars) {
                map(old_lam->var(i), vars[i]);
            } else {
                map(extra[i - old_num_vars], vars[i]);
            }
        }

        if (old_ret) {
            const Def* new_ret = new_lam->ret_var();
            map(old_ret, new_ret);
        }
    }
}

const Pi* add_extra(World& w, const Pi* ty, DefVec extra_tys) {
    auto dom       = ty->dom();
    auto num_projs = dom->num_projs();

    size_t i = 0;

    DefVec new_ty;
    while (i < num_projs) {
        auto arg_ty = dom->proj(i);

        if (arg_ty->isa<Pi>()) { break; }

        i++;
        new_ty.push_back(arg_ty);
    }

    new_ty.insert(new_ty.end(), extra_tys.begin(), extra_tys.end());

    while (i < num_projs) {
        auto arg_ty = dom->proj(i)->as<Pi>();
        i++;
        new_ty.push_back(add_extra(w, arg_ty, extra_tys));
    }

    return w.cn(new_ty);
}

const Pi* add_loop_extra(World& w, const Pi* ty, DefVec extra_tys) {
    auto dom       = ty->dom();
    auto num_projs = dom->num_projs();

    DefVec acc_ty;
    DefVec new_ty;
    new_ty.push_back(dom->proj(0));
    size_t i = 1;
    while (i < num_projs) {
        auto arg_ty = dom->proj(i);

        if (arg_ty->isa<Pi>()) { break; }

        i++;
        acc_ty.push_back(arg_ty);
    }

    acc_ty.insert(acc_ty.end(), extra_tys.begin(), extra_tys.end());

    new_ty.push_back(w.sigma(acc_ty));

    while (i < num_projs) {
        auto arg_ty = dom->proj(i)->as<Pi>();
        i++;
        new_ty.push_back(add_extra(w, arg_ty, extra_tys));
    }

    return w.cn(new_ty);
}

using DefQueue = std::deque<const Def*>;

void for_each_proj(const Def* def, std::function<void(const Def*)> collector) {
    if (def->num_projs() > 1) {
        for (auto proj : def->projs()) { for_each_proj(proj, collector); }
        return;
    }

    collector(def);
}

const Def* fill_extract_mem(const Def* def, DefQueue& vars) {
    const Def* mem = nullptr;
    for_each_proj(def, [&](const Def* def) {
        if (match<mem::M>(def->type())) {
            mem = def;
        } else {
            vars.push_back(def);
        }
    });
    return mem;
}

const Def* reshape(const Def* mem, const Def* ty, DefQueue& vars) {
    if (ty->isa<Sigma>() || ty->isa<Arr>()) {
        auto& w  = ty->world();
        auto ops = DefArray(ty->num_ops(), [&](auto i) { return reshape(mem, ty->op(i), vars); });
        return w.tuple(ty, ops);
    }

    if (match<mem::M>(ty)) { return mem; }

    assert(!vars.empty());
    const Def* next = vars.front();
    vars.pop_front();
    return next;
}

const Def* reshape(const Def* src_arg, const Def* dst_type) {
    DefQueue projs;
    const Def* mem = fill_extract_mem(src_arg, projs);
    return reshape(mem, dst_type, projs);
}

Lam* Propify::build(Lam* parent, Lam* lam) {
    if (auto new_lam = has(lam)) return new_lam->as_nom<Lam>();

    auto extra_ty   = get_extra_ty(parent);
    auto new_lam_ty = ::thorin::autodiff::add_extra(w_, lam->type()->as<Pi>(), extra_ty);

    size_t count = 0;
    for (auto dom : new_lam_ty->doms()) {
        if (match<mem::M>(dom)) { count++; }
    }

    assert(count <= 1);

    auto new_lam = w_.nom_lam(new_lam_ty, lam->dbg());

    map_global(lam, new_lam);

    push();

    new_lam->set_filter(false);
    new_lam->set_body(w_.bot(w_.type_nat()));

    auto app = lam->body()->as<App>();

    map_free_to_var(parent, lam, new_lam);
    for_each_lam(app, [&](Lam* child) { build(lam, child); });

    auto extra_test = w_.tuple(get_extra(lam));
    auto extra_arg  = rew(extra_test);
    auto new_arg    = rew(app->arg());
    const Def* new_app;
    if (match<affine::For>(app)) {
        auto [begin, end, step, acc, loop, exit] = new_arg->projs<6>();
        acc                                      = merge_tuple(acc, extra_arg->projs());
        auto wrapper_ty                          = add_loop_extra(w_, app->arg(4)->type()->as<Pi>(), get_extra_ty(lam));
        auto wrapper                             = w_.nom_lam(wrapper_ty, loop->dbg());
        const Def* wrapper_arg                   = reshape(wrapper->var(), loop->as_nom<Lam>()->dom());
        wrapper->set(loop->reduce(wrapper_arg));
        new_app = affine::op_for(w_, begin, end, step, acc->projs(), wrapper, exit);
    } else {
        auto new_extra_arg = merge_tuple(new_arg, extra_arg->projs());
        auto new_callee    = rew(app->callee());
        new_app            = w_.app(new_callee, new_extra_arg);
    }
    new_lam->set_body(new_app);
    pop();

    return new_lam;
}

void mem_first_sort(DefVec& vec) {
    std::sort(vec.begin(), vec.end(), [](const Def* left, const Def* right) {
        if (match<mem::M>(left->type())) return true;
        if (match<mem::M>(right->type())) return false;
        return left->gid() < right->gid();
    });
}

bool Propify::validate(const Def* def) {
    for (auto filter : filters) {
        if (!filter(def)) return false;
    }

    return true;
}

void Propify::add_extra(Lam* dst_lam, const Def* def) {
    if (!validate(def)) { return; }

    auto src_lam = def2nom[def];
    if (src_lam && src_lam != dst_lam) {
        assert(!def->type()->isa<Pi>());
        if (def->isa<Lit>()) return;
        extras[dst_lam].insert(def);
    } else if (def->num_projs() > 1) {
        for (auto proj : def->projs()) { add_extra(dst_lam, proj); }
    } else if (auto app = def->isa<App>()) {
        for (auto proj : app->args()) { add_extra(dst_lam, proj); }
    }
}

void Propify::meet(const Def* src, const Def* dst) {
    if (!src || !dst) return;
    auto& src_extra        = extras[src];
    auto& propagated_extra = extras_fwd[dst];
    size_t before          = propagated_extra.size();
    for (auto def : src_extra) {
        if (def2nom[def] != src) { propagated_extra.insert(def); }
    }

    size_t after = propagated_extra.size();
    if (before != after) { todo_ = true; }
}

void Propify::propagate(const CFNode* node) {
    auto lam  = node->nom()->as_nom<Lam>();
    auto body = lam->body()->as<App>();

    if (match<affine::For>(body)) {
        auto loop = body->arg(4)->as_nom<Lam>();
        auto exit = body->arg(5);

        meet(exit, lam);
        meet(loop, lam);
        meet(exit, loop->ret_var());
        meet(loop, loop->ret_var());
    } else {
        // for (auto pred : cgf().preds(node)) { meet(lam, pred->nom()); }

        auto callee = body->callee();

        if (auto extract = is_branch(callee)) {
            DefSet join;
            for (auto op : extract->tuple()->ops()) {
                auto& src_extra = extras[op];
                meet(op, lam);
                // join.insert(src_extra.begin(), src_extra.end());
            }

            // for (auto op : extract->tuple()->ops()) { extras_fwd[op].insert(join.begin(), join.end()); }
        } else {
            meet(callee, lam);
        }

        /*
                DefSet join;
                for (auto pred : cgf().preds(node)) {
                    auto& src_extra = extras_fwd[pred->nom()];
                    meet(lam, pred->nom());
                    join.insert(src_extra.begin(), src_extra.end());
                }

                for (auto pred : cgf().preds(node)) {
                    auto dst = pred->nom();
                    auto &propagated_extra = extras_fwd[dst];
                    for (auto def : join) {
                        if(mem::has_mem_ty(def->type()) && mem::has_mem_ty(dst->type())){
                            continue;
                        }

                        propagated_extra.insert(def);
                    }
                }*/
    }
}

Lam* Propify::find_lam(const Def* def) {
    if (auto extract = def->isa<Extract>()) {
        return find_lam(extract->tuple());
    } else {
        return scheduler.early(def)->as_nom<Lam>();
    }
}

Lam* Propify::build() {
    for (auto bound : scope.bound()) {
        bound->dump(1);
        if (!bound->isa<Lit>() && !bound->type()->isa<Pi>()) {
            auto lam = scheduler.early(bound);
            nom2defs[lam].insert(bound);
            def2nom[bound] = find_lam(bound);
            lams.insert(lam);
        }
    }

    for (auto dst_lam : lams) {
        for (auto def : nom2defs[dst_lam]) { add_extra(dst_lam->as_nom<Lam>(), def); }
    }

    auto& cfg = scheduler.cfg();
    todo_     = true;
    for (; todo_;) {
        todo_ = false;
        for (auto nom : cfg.reverse_post_order()) {
            if (nom != cfg.exit()) { propagate(nom); }
        }
        extras = extras_fwd;
    }

    for (auto [lam, set] : extras_fwd) {
        DefVec extra(set.begin(), set.end());
        mem_first_sort(extra);

        auto test = w_.tuple(extra);

        lam->dump();
        test->dump();

        extras_sorted.emplace(lam, extra);
    }

    auto result = build(nullptr, lam_);
    return result;
}

const Def* PropifyPass::rewrite(const Def* def) {
    if (auto ad_app = match<ad>(def); ad_app && !visited_.contains(ad_app)) {
        auto diffee = ad_app->arg()->as_nom<Lam>();

        Propify propify(diffee);
        def = op_autodiff(propify.build());
        visited_.insert(def);
    }

    return def;
}

} // namespace thorin::autodiff
