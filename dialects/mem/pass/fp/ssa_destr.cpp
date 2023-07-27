#include "dialects/mem/pass/fp/ssa_destr.h"

#include "dialects/mem/mem.h"

namespace thorin::mem {

bool SSADestr::should_destruct(Ref def) const {
    if (auto a = def->type()->isa_lit_arity()) return a >= world().flags().scalerize_threshold;
    return false;
}

void SSADestr::enter() {
    for (auto var : curr_mut()->vars()) {
        if (match<mem::M>(var->type())) {
            mem_ = var;
        } else if (should_destruct(var->type())) {
        }
    }
    // world().ILOG("hi!: {}, {}", curr_mut(),

    if (auto bb = Lam::isa_basicblock(curr_mut())) world().ILOG("bb!: {}", curr_mut());
}

Ref SSADestr::rewrite(Ref def) {
    if (auto [app, mem_lam] = isa_apped_mut_lam(def); isa_workable(mem_lam)) return phi2mem(app, mem_lam);
    return def;
}

Ref SSADestr::phi2mem(const App* app, Lam* phi_lam) {
    if (!Lam::isa_basicblock(phi_lam) || !mem_var(phi_lam)) return app;

    DefVec new_doms;
    for (auto var : phi_lam->vars())
        if (!should_destruct(var->type())) new_doms.emplace_back(var->type());

    auto mem_dom = world().sigma(new_doms);
    auto new_pi  = world().pi(mem_dom, phi_lam->codom());
    auto mem_lam = phi_lam->stub(world(), new_pi);
    auto n_phi   = phi_lam->num_vars();
    auto n_mem   = mem_lam->num_vars();

    DefVec new_vars, new_args;
    for (size_t p = 0, m = 0; p != n_phi; ++p) {
        auto arg = app->arg(n_phi, p);
        if (should_destruct(phi_lam->var(n_phi, p)->type())) {
            new_vars.emplace_back(arg);
        } else {
            new_vars.emplace_back(mem_lam->var(n_mem, m++));
            new_args.emplace_back(arg);
        }
    }

    mem_lam->set(phi_lam->reduce(world().tuple(new_vars)));
    return world().app(mem_lam, new_args);
}

Ref SSADestr::rewrite(const Proxy* proxy) { return proxy; }

undo_t SSADestr::analyze(const Proxy*) { return No_Undo; }

undo_t SSADestr::analyze(Ref) { return No_Undo; }

} // namespace thorin::mem
