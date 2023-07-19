#include "dialects/affine/passes/lower_for.h"

#include <thorin/lam.h>
#include <thorin/tuple.h>

#include "dialects/affine/affine.h"
#include "dialects/mem/mem.h"

namespace thorin::affine {

Ref LowerFor::rewrite(Ref def) {
    if (auto i = rewritten_.find(def); i != rewritten_.end()) return i->second;

    if (auto for_ax = match<affine::For>(def)) {
        world().DLOG("rewriting for axiom: {} within {}", for_ax, curr_mut());
        auto [begin, end, step, init, body, exit] = for_ax->args<6>();

        auto body_lam = body->isa_mut<Lam>();
        auto exit_lam = exit->isa_mut<Lam>();
        if (!body_lam || !exit_lam) return def;

        auto init_types = init->type()->projs();
        auto head_lam   = world().mut_lam(world().cn(merge_sigma(begin->type(), init_types)))->set("head");
        auto phis       = head_lam->vars();
        auto iter       = phis.front();
        auto acc        = world().tuple(phis.skip_front());
        auto mem_phi    = mem::mem_var(head_lam);
        auto bb_type    = world().cn(mem_phi ? mem_phi->type() : world().sigma());
        auto new_body   = world().mut_lam(bb_type)->set("new_body");
        auto new_exit   = world().mut_lam(bb_type)->set("new_exit");
        auto new_yield  = world().mut_lam(world().cn(init->type()))->set("new_yield");
        auto cmp        = world().call(core::icmp::ul, Defs{iter, end});
        auto new_iter   = world().call(core::wrap::add, core::Mode::nusw, Defs{iter, step});

        head_lam->branch(false, cmp, new_body, new_exit, mem_phi);

        auto new_yield_vars = new_yield->vars();
        new_yield->app(false, head_lam, merge_tuple(new_iter, new_yield_vars));

        new_body->set(false, body->reduce(world().tuple({iter, acc, new_yield})).back());
        new_exit->set(false, exit->reduce(acc).back());

        return rewritten_[def] = world().app(head_lam, merge_tuple(begin, init->projs()));
    }

    return def;
}

} // namespace thorin::affine
