#include "dialects/affine/passes/lower_for.h"

#include <thorin/lam.h>
#include <thorin/tuple.h>

#include "dialects/affine/affine.h"
#include "dialects/mem/mem.h"

namespace thorin::affine {

namespace {

const Def* merge_s(World& w, Ref elem, Ref sigma, Ref mem) {
    if (mem) {
        auto elems = sigma->projs();
        return merge_sigma(elem, elems);
    }
    return w.sigma({elem, sigma});
}

const Def* merge_t(World& w, Ref elem, Ref tuple, Ref mem) {
    if (mem) {
        auto elems = tuple->projs();
        return merge_tuple(elem, elems);
    }
    return w.tuple({elem, tuple});
}

} // namespace

Ref LowerFor::rewrite(Ref def) {
    if (auto i = rewritten_.find(def); i != rewritten_.end()) return i->second;

    if (auto for_ax = match<affine::For>(def)) {
        world().DLOG("rewriting for axiom: {} within {}", for_ax, curr_mut());
        auto [begin, end, step, init, body, exit] = for_ax->args<6>();

        auto body_lam = body->isa_mut<Lam>();
        auto exit_lam = exit->isa_mut<Lam>();
        if (!body_lam || !exit_lam) return def;

        auto mem       = mem::mem_def(init);
        auto head_lam  = world().mut_lam(world().cn(merge_s(world(), begin->type(), init->type(), mem)))->set("head");
        auto phis      = head_lam->vars();
        auto iter      = phis.front();
        auto acc       = world().tuple(phis.skip_front());
        mem            = mem::mem_var(head_lam);
        auto bb_type   = world().cn(mem ? mem->type() : world().sigma());
        auto new_body  = world().mut_lam(bb_type)->set("new_body");
        auto new_exit  = world().mut_lam(bb_type)->set("new_exit");
        auto new_yield = world().mut_lam(world().cn(init->type()))->set("new_yield");
        auto cmp       = world().call(core::icmp::ul, Defs{iter, end});
        auto new_iter  = world().call(core::wrap::add, core::Mode::nusw, Defs{iter, step});

        head_lam->branch(false, cmp, new_body, new_exit, mem);
        new_yield->app(false, head_lam, merge_t(world(), new_iter, new_yield->var(), mem));
        new_body->set(false, body->reduce(world().tuple({iter, acc, new_yield})).back());
        new_exit->set(false, exit->reduce(acc).back());

        return rewritten_[def] = world().app(head_lam, merge_t(world(), begin, init, mem));
    }

    return def;
}

} // namespace thorin::affine
