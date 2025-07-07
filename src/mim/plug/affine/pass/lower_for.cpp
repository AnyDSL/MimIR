#include "mim/plug/affine/pass/lower_for.h"

#include <mim/lam.h>
#include <mim/tuple.h>

#include <mim/plug/mem/mem.h>

#include "mim/plug/affine/affine.h"

namespace mim::plug::affine {

namespace {

const Def* merge_s(World& w, const Def* elem, const Def* sigma, const Def* mem) {
    if (mem) {
        auto elems = sigma->projs();
        return merge_sigma(elem, elems);
    }
    return w.sigma({elem, sigma});
}

const Def* merge_t(World& w, const Def* elem, const Def* tuple, const Def* mem) {
    if (mem) {
        auto elems = tuple->projs();
        return merge_tuple(elem, elems);
    }
    return w.tuple({elem, tuple});
}

const Def* eta_expand(World& w, const Def* f) {
    auto eta = w.mut_con(Pi::isa_cn(f->type())->dom());
    eta->app(false, f, eta->var());
    return eta;
}

} // namespace

const Def* LowerFor::rewrite(const Def* def) {
    if (auto i = rewritten_.find(def); i != rewritten_.end()) return i->second;

    if (auto for_ax = Axm::isa<affine::For>(def)) {
        world().DLOG("rewriting for axm: {} within {}", for_ax, curr_mut());
        auto [begin, end, step, init, body, exit] = for_ax->args<6>();

        auto body_lam = body->isa_mut<Lam>();
        auto exit_lam = exit->isa_mut<Lam>();
        if (!body_lam) body = eta_expand(world(), body);
        if (!exit_lam) exit = eta_expand(world(), exit);

        auto mem       = mem::mem_def(init);
        auto head_lam  = world().mut_con(merge_s(world(), begin->type(), init->type(), mem))->set("head");
        auto phis      = head_lam->vars();
        auto iter      = phis.front();
        auto acc       = world().tuple(phis.view().subspan(1));
        mem            = mem::mem_var(head_lam);
        auto bb_dom    = mem ? mem->type() : world().sigma();
        auto new_body  = world().mut_con(bb_dom)->set("new_body");
        auto new_exit  = world().mut_con(bb_dom)->set("new_exit");
        auto new_yield = world().mut_con(init->type())->set("new_yield");
        auto cmp       = world().call(core::icmp::ul, Defs{iter, end});
        auto new_iter  = world().call(core::wrap::add, core::Mode::nusw, Defs{iter, step});

        head_lam->branch(false, cmp, new_body, new_exit, mem);
        new_yield->app(false, head_lam, merge_t(world(), new_iter, new_yield->var(), mem));
        new_body->set(body->reduce(world().tuple({iter, acc, new_yield})));
        new_exit->set(exit->reduce(acc));

        return rewritten_[def] = world().app(head_lam, merge_t(world(), begin, init, mem));
    }

    return def;
}

} // namespace mim::plug::affine
