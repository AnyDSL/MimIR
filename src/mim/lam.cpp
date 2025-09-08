#include "mim/lam.h"

#include "mim/world.h"

using namespace std::string_literals;

namespace mim {

/*
 * Pi
 */

const Pi* Pi::ret_pi() const {
    if (num_doms() > 0) {
        auto ret = dom(num_doms() - 1);
        if (auto pi = Pi::isa_basicblock(ret)) return pi;
    }

    return nullptr;
}

Pi* Pi::set_dom(Defs doms) { return Def::set(0, world().sigma(doms))->as<Pi>(); }

/*
 * Lam
 */

Lam* Lam::set_filter(Filter filter) { return Def::set(0, world().filter(filter))->as<Lam>(); }
Lam* Lam::set(Filter filter, const Def* body) { return Def::set({world().filter(filter), body})->as<Lam>(); }
Lam* Lam::app(Filter f, const Def* callee, const Def* arg) {
    return Def::set({world().filter(f), world().app(callee, arg)})->as<Lam>();
}
Lam* Lam::app(Filter filter, const Def* callee, Defs args) { return app(filter, callee, world().tuple(args)); }

Lam* Lam::branch(Filter filter, const Def* cond, const Def* t, const Def* f, const Def* arg) {
    return app(filter, world().select(cond, t, f), arg ? arg : world().tuple());
}

// TODO maybe we can eta-reduce immutable Lams in some edge casess like: lm _: [] = f ();

const Def* Lam::eta_reduce() const {
    if (auto var = has_var()) {
        if (auto app = body()->isa<App>())
            if (app->arg() == var && !app->callee()->free_vars().contains(var)) return app->callee();
    }
    return nullptr;
}

const Def* Lam::eta_expand(Filter filter, const Def* f) {
    auto& w  = f->world();
    auto eta = w.mut_lam(f->type()->as<Pi>());
    eta->debug_suffix("eta_"s + f->sym().str());
    return eta->app(filter, f, eta->var());
}

/*
 * Helpers
 */

const Def* compose_cn(const Def* f, const Def* g) {
    auto& world = f->world();
    world.DLOG("compose f (B->C): {} : {}", f, f->type());
    world.DLOG("compose g (A->B): {} : {}", g, g->type());

    auto F = f->type()->as<Pi>();
    auto G = g->type()->as<Pi>();

    assert(Pi::isa_returning(F));
    assert(Pi::isa_returning(G));

    auto A = G->dom(2, 0);
    auto B = G->ret_dom();
    auto C = F->ret_dom();
    // The type check of codom G = dom F is better handled by the application type checking

    world.DLOG("compose f (B->C): {} : {}", f, F);
    world.DLOG("compose g (A->B): {} : {}", g, G);
    world.DLOG("  A: {}", A);
    world.DLOG("  B: {}", B);
    world.DLOG("  C: {}", C);

    auto h     = world.mut_fun(A, C)->set("comp_"s + f->sym().str() + "_"s + g->sym().str());
    auto hcont = world.mut_con(B)->set("comp_"s + f->sym().str() + "_"s + g->sym().str() + "_cont"s);

    h->app(true, g, {h->var((nat_t)0), hcont});

    auto hcont_var = hcont->var(); // Warning: not var(0) => only one var => normalization flattens tuples down here.
    hcont->app(true, f, {hcont_var, h->var(1) /* ret_var */});

    return h;
}

} // namespace mim
