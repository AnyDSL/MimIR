#include "dialects/mem/pass/fp/ssa_constr.h"

#include "thorin/pass/fp/eta_exp.h"

#include "dialects/mem/mem.h"

namespace thorin::mem {

namespace {
Ref get_sloxy_type(const Proxy* sloxy) { return force<mem::Ptr>(sloxy->type())->arg(0); }

std::tuple<const Proxy*, Lam*> split_phixy(const Proxy* phixy) {
    return {phixy->op(0)->as<Proxy>(), phixy->op(1)->as_mut<Lam>()};
}
} // namespace

void SSAConstr::enter() { lam2sloxy2val_[curr_mut()].clear(); }

Ref SSAConstr::rewrite(const Proxy* proxy) {
    if (proxy->tag() == Traxy) {
        world().DLOG("traxy '{}'", proxy);
        for (size_t i = 1, e = proxy->num_ops(); i != e; i += 2)
            set_val(curr_mut(), as_proxy(proxy->op(i), Sloxy), proxy->op(i + 1));
        return proxy->op(0);
    }

    return proxy;
}

Ref SSAConstr::rewrite(Ref def) {
    if (auto slot = match<mem::slot>(def)) {
        auto [mem, id] = slot->args<2>();
        auto [_, ptr]  = slot->projs<2>();
        auto sloxy     = proxy(ptr->type(), {curr_mut(), id}, Sloxy)->set(slot->dbg());
        world().DLOG("sloxy: '{}'", sloxy);
        if (!keep_.contains(sloxy)) {
            set_val(curr_mut(), sloxy, world().bot(get_sloxy_type(sloxy)));
            data(curr_mut()).writable.emplace(sloxy);
            return world().tuple({mem, sloxy});
        }
    } else if (auto load = match<mem::load>(def)) {
        auto [mem, ptr] = load->args<2>();
        if (auto sloxy = isa_proxy(ptr, Sloxy)) return world().tuple({mem, get_val(curr_mut(), sloxy)});
    } else if (auto store = match<mem::store>(def)) {
        auto [mem, ptr, val] = store->args<3>();
        if (auto sloxy = isa_proxy(ptr, Sloxy)) {
            if (data(curr_mut()).writable.contains(sloxy)) {
                set_val(curr_mut(), sloxy, val);
                return op_remem(mem)->set(store->dbg());
            }
        }
    } else if (auto [app, mem_lam] = isa_apped_mut_lam(def); isa_workable(mem_lam)) {
        return mem2phi(app, mem_lam);
    } else {
        for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
            if (auto lam = def->op(i)->isa_mut<Lam>(); isa_workable(lam)) {
                if (mem2phi_.contains(lam)) return def->refine(i, eta_exp_->proxy(lam));
            }
        }
    }

    return def;
}

Ref SSAConstr::get_val(Lam* lam, const Proxy* sloxy) {
    auto& sloxy2val = lam2sloxy2val_[lam];
    if (auto i = sloxy2val.find(sloxy); i != sloxy2val.end()) {
        auto val = i->second;
        world().DLOG("get_val found: '{}': '{}': '{}'", sloxy, val, lam);
        return val;
    } else if (lam->is_external()) {
        world().DLOG("cannot install phi for '{}' in '{}'", sloxy, lam);
        return sloxy;
    } else if (auto pred = data(lam).pred) {
        world().DLOG("get_val recurse: '{}': '{}' -> '{}'", sloxy, pred, lam);
        return get_val(pred, sloxy);
    } else {
        auto phixy = proxy(get_sloxy_type(sloxy), {sloxy, lam}, Phixy)->set(sloxy->dbg());
        phixy->debug_prefix("_phi_");
        world().DLOG("get_val phixy: '{}' '{}'", sloxy, lam);
        return set_val(lam, sloxy, phixy);
    }
}

Ref SSAConstr::set_val(Lam* lam, const Proxy* sloxy, Ref val) {
    world().DLOG("set_val: '{}': '{}': '{}'", sloxy, val, lam);
    return lam2sloxy2val_[lam][sloxy] = val;
}

Ref SSAConstr::mem2phi(const App* app, Lam* mem_lam) {
    auto&& sloxys = lam2sloxys_[mem_lam];
    if (sloxys.empty()) return app;

    DefVec types, phis;
    for (auto i = sloxys.begin(), e = sloxys.end(); i != e;) {
        auto sloxy = *i;
        if (keep_.contains(sloxy)) {
            i = sloxys.erase(i);
        } else {
            phis.emplace_back(sloxy);
            types.emplace_back(get_sloxy_type(sloxy));
            ++i;
        }
    }

    size_t num_phis = phis.size();
    if (num_phis == 0) return app;

    auto&& [phi_lam, old_phis] = mem2phi_[mem_lam];
    if (phi_lam == nullptr || old_phis != phis) {
        old_phis      = phis;
        auto new_type = world().pi(merge_sigma(mem_lam->dom(), types), mem_lam->codom());
        phi_lam       = world().mut_lam(new_type)->set(mem_lam->dbg());
        eta_exp_->new2old(phi_lam, mem_lam);
        world().DLOG("new phi_lam '{}'", phi_lam);

        auto num_mem_vars = mem_lam->num_vars();
        DefVec traxy_ops(2 * num_phis + 1);
        traxy_ops[0] = phi_lam->var();
        for (size_t i = 0; auto sloxy : sloxys) {
            traxy_ops[2 * i + 1] = sloxy;
            traxy_ops[2 * i + 2] = phi_lam->var(num_mem_vars + i);
            ++i;
        }
        auto traxy = proxy(phi_lam->var()->type(), traxy_ops, Traxy);

        auto new_vars = DefVec(num_mem_vars, [&](size_t i) { return traxy->proj(i); });
        phi_lam->set(mem_lam->reduce(world().tuple(mem_lam->dom(), new_vars)));
    } else {
        world().DLOG("reuse phi_lam '{}'", phi_lam);
    }

    world().DLOG("mem_lam => phi_lam: '{}': '{}' => '{}': '{}'", mem_lam, mem_lam->type()->dom(), phi_lam,
                 phi_lam->dom());
    auto sloxy = sloxys.begin();
    auto args  = DefVec(num_phis, [&](auto) { return get_val(curr_mut(), *sloxy++); });
    return world().app(phi_lam, merge_tuple(app->arg(), args));
}

undo_t SSAConstr::analyze(const Proxy* proxy) {
    if (proxy->tag() == Sloxy) {
        auto sloxy_lam = proxy->op(0)->as_mut<Lam>();

        if (keep_.emplace(proxy).second) {
            world().DLOG("keep: '{}'; pointer needed", proxy);
            return undo_enter(sloxy_lam);
        }
    }

    assert(proxy->tag() == Phixy);
    auto [sloxy, mem_lam] = split_phixy(proxy);
    if (lam2sloxys_[mem_lam].emplace(sloxy).second) {
        world().DLOG("phi needed: phixy '{}' for sloxy '{}' for mem_lam '{}'", proxy, sloxy, mem_lam);
        return undo_visit(mem_lam);
    }

    return No_Undo;
}

undo_t SSAConstr::analyze(Ref def) {
    for (size_t i = 0, e = def->num_ops(); i != e; ++i) {
        if (auto succ_lam = isa_workable(def->op(i)->isa_mut<Lam>())) {
            auto& succ_info = data(succ_lam);

            // TODO this is a bit scruffy - maybe we can do better
            if (Lam::isa_basicblock(succ_lam) && succ_lam != curr_mut())
                for (auto writable = data(curr_mut()).writable; auto&& w : writable) succ_info.writable.insert(w);

            if (!isa_callee(def, i)) {
                if (succ_info.pred) {
                    world().DLOG("several preds in non-callee position; wait for EtaExp");
                    succ_info.pred = nullptr;
                } else {
                    world().DLOG("'{}' -> '{}'", curr_mut(), succ_lam);
                    succ_info.pred = curr_mut();
                }
            }
        }
    }

    return No_Undo;
}

} // namespace thorin::mem
