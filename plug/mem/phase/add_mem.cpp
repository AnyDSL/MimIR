#include "add_mem.h"

#include <memory>

#include "thorin/analyses/schedule.h"

#include "plug/mem/mem.h"

namespace thorin::mem {

namespace {

std::pair<const App*, Vector<Lam*>> isa_apped_mut_lam_in_tuple(const Def* def) {
    if (auto app = def->isa<App>()) {
        Vector<Lam*> lams;
        std::deque<const Def*> wl;
        wl.push_back(app->callee());
        while (!wl.empty()) {
            auto elem = wl.front();
            wl.pop_front();
            if (auto mut = elem->isa_mut<Lam>()) {
                lams.push_back(mut);
            } else if (auto extract = elem->isa<Extract>()) {
                if (auto tuple = extract->tuple()->isa<Tuple>())
                    for (auto&& op : tuple->ops()) wl.push_back(op);
                else
                    return {nullptr, {}};
            } else {
                return {nullptr, {}};
            }
        }
        return {app, lams};
    }
    return {nullptr, {}};
}

// @pre isa_apped_mut_lam_in_tuple(def) valid
template<class F, class H> const Def* rewrite_mut_lam_in_tuple(const Def* def, F&& rewrite, H&& rewrite_idx) {
    auto& w = def->world();
    if (auto mut = def->isa_mut<Lam>()) return std::forward<F>(rewrite)(mut);

    auto extract = def->as<Extract>();
    auto tuple   = extract->tuple()->as<Tuple>();
    auto new_ops = DefVec(tuple->ops(), [&](const Def* op) {
        return rewrite_mut_lam_in_tuple(op, std::forward<F>(rewrite), std::forward<H>(rewrite_idx));
    });
    return w.extract(w.tuple(new_ops), rewrite_idx(extract->index()));
}

// @pre isa_apped_mut_lam_in_tuple(def) valid
template<class RewriteCallee, class RewriteArg, class RewriteIdx>
const Def* rewrite_apped_mut_lam_in_tuple(const Def* def,
                                          RewriteCallee&& rewrite_callee,
                                          RewriteArg&& rewrite_arg,
                                          RewriteIdx&& rewrite_idx) {
    auto& w     = def->world();
    auto app    = def->as<App>();
    auto callee = rewrite_mut_lam_in_tuple(app->callee(), std::forward<RewriteCallee>(rewrite_callee),
                                           std::forward<RewriteIdx>(rewrite_idx));
    auto arg    = std::forward<RewriteArg>(rewrite_arg)(app->arg());
    return app->rebuild(w, app->type(), {callee, arg});
}

} // namespace

// Entry point of the phase.
void AddMem::visit(const Scope& scope) {
    if (auto entry = scope.entry()->isa_mut<Lam>()) {
        scope.free_muts(); // cache this.
        sched_ = Scheduler{scope};
        add_mem_to_lams(entry, entry);
    }
}

const Def* AddMem::mem_for_lam(Lam* lam) const {
    if (auto it = mem_rewritten_.find(lam); it != mem_rewritten_.end()) {
        // We created a new lambda. Therefore, we want to lookup the mem for the new lambda.
        lam = it->second->as_mut<Lam>();
    }
    if (auto it = val2mem_.find(lam); it != val2mem_.end()) {
        lam->world().DLOG("found mem for {} in val2mem_ : {}", lam, it->second);
        // We found a (overwritten) memory in the lambda.
        return it->second;
    }
    // As a fallback, we lookup the memory in vars of the lambda.
    auto mem = mem::mem_var(lam);
    assert(mem && "mut must have mem!");
    return mem;
}

const Def* AddMem::rewrite_type(const Def* type) {
    if (auto pi = type->isa<Pi>()) return rewrite_pi(pi);

    if (auto it = mem_rewritten_.find(type); it != mem_rewritten_.end()) return it->second;

    auto new_ops                = DefVec(type->num_ops(), [&](size_t i) { return rewrite_type(type->op(i)); });
    return mem_rewritten_[type] = type->rebuild(world(), type->type(), new_ops);
}

const Def* AddMem::rewrite_pi(const Pi* pi) {
    if (auto it = mem_rewritten_.find(pi); it != mem_rewritten_.end()) return it->second;

    auto dom     = pi->dom();
    auto new_dom = DefVec(dom->num_projs(), [&](size_t i) { return rewrite_type(dom->proj(i)); });
    if (pi->num_doms() == 0 || !match<mem::M>(pi->dom(0_s))) {
        new_dom
            = DefVec(dom->num_projs() + 1, [&](size_t i) { return i == 0 ? world().annex<mem::M>() : new_dom[i - 1]; });
    }

    return mem_rewritten_[pi] = world().pi(new_dom, pi->codom());
}

const Def* AddMem::add_mem_to_lams(Lam* curr_lam, const Def* def) {
    auto place = static_cast<Lam*>(sched_.smart(def));

    // world().DLOG("rewriting {} : {} in {}", def, def->type(), place);

    if (auto global = def->isa<Global>()) return global;
    if (auto mut_lam = def->isa_mut<Lam>(); mut_lam && !mut_lam->is_set()) return def;
    if (auto ax = def->isa<Axiom>()) return ax;
    if (auto it = mem_rewritten_.find(def); it != mem_rewritten_.end()) {
        auto tmp = it->second;
        if (match<mem::M>(def->type())) {
            world().DLOG("already known mem {} in {}", def, curr_lam);
            auto new_mem = mem_for_lam(curr_lam);
            world().DLOG("new mem {} in {}", new_mem, curr_lam);
            return new_mem;
        }
        if (curr_lam != def) {
            // world().DLOG("rewritten def: {} : {} in {}", tmp, tmp->type(), curr_lam);
            return tmp;
        }
    }
    if (match<mem::M>(def->type())) world().DLOG("new mem {} in {}", def, curr_lam);

    auto rewrite_lam = [&](Lam* lam) -> const Def* {
        auto pi      = lam->type()->as<Pi>();
        auto new_lam = lam;

        if (auto it = mem_rewritten_.find(lam); it != mem_rewritten_.end()) {
            if (curr_lam == lam) // i.e. we've stubbed this, but now we rewrite it
                new_lam = it->second->as_mut<Lam>();
            else if (auto pi = it->second->type()->as<Pi>(); pi->num_doms() > 0 && match<mem::M>(pi->dom(0_s)))
                return it->second;
        }

        if (!lam->is_set()) return lam;
        world().DLOG("rewrite lam {}", lam);

        bool is_bound = sched_.scope().bound(lam) || lam == curr_lam;

        if (new_lam == lam) // if not stubbed yet
            if (auto new_pi = rewrite_pi(pi); new_pi != pi) new_lam = lam->stub(world(), new_pi);

        if (!is_bound) {
            world().DLOG("free lam {}", lam);
            mem_rewritten_[lam] = new_lam;
            return new_lam;
        }

        auto var_offset = new_lam->num_doms() - lam->num_doms(); // have we added a mem var?
        if (lam->num_vars() != 0) mem_rewritten_[lam->var()] = new_lam->var();
        for (size_t i = 0; i < lam->num_vars() && new_lam->num_vars() > 1; ++i)
            mem_rewritten_[lam->var(i)] = new_lam->var(i + var_offset);

        auto var                = new_lam->var(0_n);
        mem_rewritten_[new_lam] = new_lam;
        mem_rewritten_[lam]     = new_lam;
        val2mem_[new_lam]       = var;
        val2mem_[lam]           = var;
        mem_rewritten_[var]     = var;
        auto filter             = add_mem_to_lams(lam, lam->filter());
        auto body               = add_mem_to_lams(lam, lam->body());
        new_lam->unset()->set({filter, body});

        if (lam != new_lam && lam->is_external()) lam->transfer_external(new_lam);
        return new_lam;
    };

    // rewrite top-level lams
    if (auto lam = def->isa_mut<Lam>()) return rewrite_lam(lam);
    assert(!def->isa_mut());

    if (auto pi = def->isa<Pi>()) return rewrite_pi(pi);

    auto rewrite_arg = [&](const Def* arg) -> const Def* {
        size_t offset = (arg->type()->num_projs() > 0 && match<mem::M>(arg->type()->proj(0))) ? 0 : 1;
        if (offset == 0) {
            // depth-first, follow the mems
            add_mem_to_lams(place, arg->proj(0));
        }

        DefVec new_args{arg->type()->num_projs() + offset};
        for (int i = new_args.size() - 1; i >= 0; i--) {
            new_args[i]
                = i == 0 ? add_mem_to_lams(place, mem_for_lam(place)) : add_mem_to_lams(place, arg->proj(i - offset));
        }
        return arg->world().tuple(new_args);
    };

    // call-site of a mutable lambda
    if (auto apped_mut = isa_apped_mut_lam_in_tuple(def); apped_mut.first) {
        return mem_rewritten_[def]
             = rewrite_apped_mut_lam_in_tuple(def, std::move(rewrite_lam), std::move(rewrite_arg),
                                              [&](const Def* def) { return add_mem_to_lams(place, def); });
    }

    // call-site of a continuation
    if (auto app = def->isa<App>(); app && (app->callee()->dep() & Dep::Var)) {
        return mem_rewritten_[def]
             = app->rebuild(world(), app->type(), {add_mem_to_lams(place, app->callee()), rewrite_arg(app->arg())});
    }

    // call-site of an axiom (assuming mems are only in the final app..)
    // assume all "negative" curry depths are fully applied axioms, so we do not want to rewrite those here..
    if (auto app = def->isa<App>(); app && app->axiom() && app->curry() ^ 0x8000) {
        auto arg = app->arg();
        DefVec new_args(arg->num_projs());
        for (int i = new_args.size() - 1; i >= 0; i--) {
            // replace memory operand with followed mem
            if (match<mem::M>(arg->proj(i)->type())) {
                // depth-first, follow the mems
                add_mem_to_lams(place, arg->proj(i));
                new_args[i] = add_mem_to_lams(place, mem_for_lam(place));
            } else {
                new_args[i] = add_mem_to_lams(place, arg->proj(i));
            }
        }
        auto rewritten = mem_rewritten_[def]
            = app->rebuild(world(), app->type(),
                           {add_mem_to_lams(place, app->callee()), world().tuple(new_args)->set(arg->dbg())})
                  ->set(app->dbg());
        if (match<mem::M>(rewritten->type())) {
            world().DLOG("memory from axiom {} : {}", rewritten, rewritten->type());
            val2mem_[place] = rewritten;
        }
        if (rewritten->num_projs() > 0 && match<mem::M>(rewritten->proj(0)->type())) {
            world().DLOG("memory from axiom 2 {} : {}", rewritten, rewritten->type());
            mem_rewritten_[rewritten->proj(0)] = rewritten->proj(0);
            val2mem_[place]                    = rewritten->proj(0);
        }
        return rewritten;
    }

    // all other apps: when rewriting the callee adds a mem to the doms, add a mem to the arg as well..
    if (auto app = def->isa<App>()) {
        auto new_callee = add_mem_to_lams(place, app->callee());
        auto new_arg    = add_mem_to_lams(place, app->arg());
        if (app->callee()->type()->as<Pi>()->num_doms() + 1 == new_callee->type()->as<Pi>()->num_doms())
            new_arg = rewrite_arg(app->arg());
        auto rewritten = mem_rewritten_[def]
            = app->rebuild(world(), app->type(), {new_callee, new_arg})->set(app->dbg());
        if (match<mem::M>(rewritten->type())) {
            world().DLOG("memory from other {} : {}", rewritten, rewritten->type());
            val2mem_[place] = rewritten;
        }
        if (rewritten->num_projs() > 0 && match<mem::M>(rewritten->proj(0)->type())) {
            world().DLOG("memory from other 2 {} : {}", rewritten, rewritten->type());
            mem_rewritten_[rewritten->proj(0)] = rewritten->proj(0);
            val2mem_[place]                    = rewritten->proj(0);
        }
        return rewritten;
    }

    auto new_ops = DefVec(def->ops(), [&](const Def* op) {
        if (match<mem::M>(op->type())) {
            // depth-first, follow the mems
            add_mem_to_lams(place, op);
            return add_mem_to_lams(place, mem_for_lam(place));
        }
        return add_mem_to_lams(place, op);
    });

    auto tmp = mem_rewritten_[def] = def->rebuild(world(), rewrite_type(def->type()), new_ops)->set(def->dbg());
    // if (match<mem::M>(tmp->type())) {
    //     world().DLOG("memory from other op 1 {} : {}", tmp, tmp->type());
    //     val2mem_[place] = tmp;
    // }
    // if (tmp->num_projs() > 0 && match<mem::M>(tmp->proj(0)->type())) {
    //     world().DLOG("memory from other op 2 {} : {}", tmp, tmp->type());
    //     mem_rewritten_[tmp->proj(0)] = tmp->proj(0);
    //     val2mem_[place]              = tmp->proj(0);
    // }
    return tmp;
}

} // namespace thorin::mem
