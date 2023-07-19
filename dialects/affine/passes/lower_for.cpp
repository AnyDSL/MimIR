#include "dialects/affine/passes/lower_for.h"

#include <thorin/lam.h>
#include <thorin/tuple.h>

#include "dialects/affine/affine.h"
#include "dialects/mem/mem.h"

namespace thorin::affine {

Ref LowerFor::rewrite(Ref def) {
    if (auto i = rewritten_.find(def); i != rewritten_.end()) return i->second;

    if (auto for_ax = match<affine::For>(def)) {
        auto& w = world();
        w.DLOG("rewriting for axiom: {} within {}", for_ax, curr_mut());
        auto [begin, end, step, init, body, exit] = for_ax->args<6>();

        auto body_lam = body->isa_mut<Lam>();
        auto exit_lam = exit->isa_mut<Lam>();
        if (!body_lam || !exit_lam) return def;

        auto head_cn    = world().cn(merge_sigma(begin->type(), init->type()->projs()));
        auto head_lam   = world().mut_lam(head_cn)->set("head");
        auto phis       = head_lam->vars();
        auto iter       = phis.front();
        auto accs       = phis.skip_front();
        auto acc        = world().tuple(accs);
        auto mem_phi    = mem::mem_var(head_lam);
        auto bb_type    = world().cn(mem_phi ? mem_phi->type() : world().sigma());
        auto new_body   = world().mut_lam(bb_type)->set("body_");
        auto new_exit   = world().mut_lam(bb_type)->set("exit_");
        auto new_yield  = world().mut_lam(world().cn(init->type()))->set("yield_");
        iter            = w.call(core::wrap::add, core::Mode::nusw, Defs{iter, step});
        auto cmp        = w.call(core::icmp::ul, Defs{iter, end});
        auto iter_acc   = merge_tuple(iter, new_yield->vars());
        auto begin_init = merge_tuple(begin, init->projs());

        head_lam->branch(false, cmp, new_body, new_exit, mem_phi);
        new_exit->set(false, exit->reduce(acc).back());
        new_yield->app(false, head_lam, iter_acc);
        new_body->set(false, body->reduce(world().tuple({iter, acc, new_yield})).back());

        return rewritten_[def] = world().app(head_lam, begin_init);
    }

    return def;
}

} // namespace thorin::affine
