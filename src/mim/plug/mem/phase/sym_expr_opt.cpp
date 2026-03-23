#include "mim/plug/mem/phase/sym_expr_opt.h"

#include <absl/container/fixed_array.h>

#include "mim/plug/mem/mem.h"

namespace mim::plug::mem::phase {

#define PROXY_GVN 0
#define PROXY_MEM 1
#define PROXY_SLOT 2

const Def* isa_mem_proxy(const Def* def) {
    if (auto mem = def->isa<Proxy>())
        if (mem->tag() == PROXY_MEM)
            return mem;
    return nullptr;
}

const Def* isa_gvn_proxy(const Def* def) {
    if (auto mem = def->isa<Proxy>())
        if (mem->tag() == PROXY_GVN)
            return mem;
    return nullptr;
}

Def* SymExprOpt::Analysis::rewrite_mut(Def* mut) {
    map(mut, mut);

    if (auto var = mut->has_var()) {
        map(var, var);

        if (mut->isa<Lam>())
            for (auto var : mut->tvars()) {
                map(var, var);
                lattice_[var] = var;
            }
    }

    for (auto d : mut->deps())
        rewrite(d);

    return mut;
}

const Def* SymExprOpt::Analysis::propagate(const Def* var, const Def* def) {
    ELOG("called propagate: {} → {}", var, def);
    auto [i, ins] = lattice_.emplace(var, def);
    if (ins) {
        todo_ = true;
        ELOG("propagate: {} → {}", var, def);
        return def;
    }

    auto cur = i->second;
    if (!cur || def->isa<Bot>() || cur == def || cur == var || isa_gvn_proxy(cur)) return cur;

    todo_ = true;
    DLOG("cannot propagate {}, trying GVN", var);
    if (cur->isa<Bot>()) return i->second = def;
    ELOG("marking {} for gvn", var);
    return i->second = nullptr; // we reached top for propagate; nullptr marks this to bundle for GVN
}

static nat_t get_index(const Def* def) { return Lit::as(def->as<Extract>()->index()); }

const Def* mem_proxy_set(const Def* mem, const Def* key, const Def* value) {
    auto& world    = mem->world();
    auto new_entry = world.tuple({key, value});
    if (auto proxy = isa_mem_proxy(mem)) {
        auto new_map_entries = DefVec();
        new_map_entries.push_back(mem->op(0));
        for (auto kv : proxy->ops() | std::views::drop(1)) {
            auto [k, v] = kv->projs<2>();
            if (k != key) new_map_entries.push_back(kv);
        }
        new_map_entries.push_back(new_entry);
        return world.proxy(world.annex<mem::M>(), new_map_entries, 0, PROXY_MEM);
    }
    return world.proxy(world.annex<mem::M>(), {mem, new_entry}, 0, PROXY_MEM);
}

const Def* mem_proxy_empty(const Def* mem) {
    auto& world    = mem->world();
    return world.proxy(world.annex<mem::M>(), {mem}, 0, PROXY_MEM);
}

std::pair<const Def*, bool> mem_proxy_emplace(const Def* mem, const Def* key, const Def* value) {
    auto& world    = mem->world();
    auto new_entry = world.tuple({key, value});
    if (auto proxy = isa_mem_proxy(mem)) {
        bool already_contained = false;
        auto new_map_entries = DefVec();
        new_map_entries.push_back(mem->op(0));
        for (auto kv : proxy->ops() | std::views::drop(1)) {
            auto [k, v] = kv->projs<2>();
            if (k == key) already_contained = true;
            new_map_entries.push_back(kv);
        }
        if (!already_contained) new_map_entries.push_back(new_entry);
        return {world.proxy(world.annex<mem::M>(), new_map_entries, 0, PROXY_MEM), !already_contained};
    }
    return {world.proxy(world.annex<mem::M>(), {mem, new_entry}, 0, PROXY_MEM), true};
}

const Def* mem_proxy_lookup(const Def* mem, const Def* key) {
    if (auto proxy = isa_mem_proxy(mem)) {
        for (auto kv : proxy->ops() | std::views::drop(1)) {
            auto [k, v] = kv->projs<2>();
            if (k == key) return v;
        }
    }
    return nullptr;
}

const Def* SymExprOpt::Analysis::trace_load(const Def* mem, const Def* ptr) {
    if (auto store = Axm::isa<mem::store>(mem)) {
        auto [mem, assigned_ptr, val] = store->args<3>();
        if (assigned_ptr == ptr) {
            ELOG("tracing found the store for {}, value is {}", ptr, val);
            return val;
        }
        else
            return trace_load(mem, ptr);
    } else if (auto load = Axm::isa<mem::load>(mem)) {
        auto [mem, _] = load->args<2>();
        return trace_load(mem, ptr);
    } else if (auto slot = Axm::isa<mem::slot>(mem)) {
        auto [Ta, mi]                      = slot->uncurry_args<2>();
        auto [pointee_type, address_space] = Ta->projs<2>();
        auto [mem, alloced_ptr]            = mi->projs<2>();
        if (ptr == alloced_ptr) return world().bot(pointee_type);
        return trace_load(mem, ptr);
    } else if (auto extract = mem->isa<Extract>()) {
        return trace_load(extract->tuple(), ptr);
    } else {
        if (auto var = mem->isa<Var>()) {
            // todo: introduce new var here
            auto pointee_type = ptr->type()->op(1)->op(0);
            auto [i, ins_lattice] = lattice_.emplace(var->mut(), mem_proxy_empty(var));
            auto slot_proxy = mem->world().proxy(pointee_type, {}, 0, PROXY_SLOT);
            auto [mem_proxy, ins] = mem_proxy_emplace(i->second, ptr, slot_proxy);
            i->second = mem_proxy;
            if (ins) {
                ELOG("setting todo because of new live slot");
                todo_ = true;
            }
        }
        return nullptr;
    }
}

const Def* SymExprOpt::Analysis::rewrite_imm_App(const App* app) {
    if (auto store = Axm::isa<mem::store>(app)) {
        auto [mem, ptr, val] = store->args<3>();
        auto abstr_mem = rewrite(mem);
        auto abstr_ptr = rewrite(ptr);
        auto abstr_val = rewrite(val);
        ELOG("found a store: {} <- {}", abstr_ptr, val);
    } else if (auto load = Axm::isa<mem::load>(app)) {
        auto [mem, ptr] = load->args<2>();
        auto [_, loaded] = load->projs<2>();
        // auto abstr_mem = rewrite(mem);
        // auto abstr_ptr = rewrite(ptr);
        if (auto value = trace_load(mem, ptr)) {
            map(loaded, value);
        }
        ELOG("found a load, for {}, value is {}", ptr, trace_load(mem, ptr));
        ELOG("after the trace, lattice:");
        for (auto [k, v] : lattice_)
            if (k != v)
                ELOG("{} -> {v}", k, v);
    } else if (auto slot = Axm::isa<mem::slot>(app)) {
        ELOG("found a slot");
        auto [mem, id] = slot->args<2>();
        auto [_, ptr]  = slot->projs<2>();
        auto abstr_mem = rewrite(mem);
        auto abstr_id = rewrite(id);
    } else if (auto lam = app->callee()->isa_mut<Lam>(); isa_optimizable(lam)) {
        ELOG("live_slots for {}:", lam);
        for (auto [slot, thing] : live_slots_[lam])
            ELOG("{} {}", slot, thing);

        auto n          = app->num_targs();
        auto abstr_args = absl::FixedArray<const Def*>(n);
        auto abstr_vars = absl::FixedArray<const Def*>(n);

        DefVec slot_vars;
        DefVec slot_args;
        // propagate
        for (size_t i = 0; i != app->num_targs(); ++i) {
            // real vars
            auto abstr    = rewrite(app->targ(i));
            abstr_vars[i] = propagate(lam->tvar(i), abstr);
            abstr_args[i] = abstr;
            ELOG("propagating for arg {}", app->targ(i));
            if (Axm::isa<mem::M>(app->targ(i)->type())) {
                ELOG("found mem, propagating stuff");
                // found a mem arg, propagate mem slots
                if (auto i_mem = lattice_.find(lam); i_mem != lattice_.end()) {
                    auto proxy = i_mem->second;
                    for (auto kv : proxy->ops() | std::views::drop(1)) {
                        auto [slot, val] = kv->projs<2>();
                        auto traced = trace_load(app->targ(i), slot);
                        auto abstr = rewrite(traced ? traced: world().bot(val->type()));
                        slot_vars.push_back(propagate(val, abstr));
                        slot_args.push_back(abstr);
                    }
                }

                ELOG("slot_vars for {}: {}", lam, slot_vars);
                ELOG("slot_args for {}: {}", lam, slot_args);
            }
        }

        // GVN bundle: All things marked as top (nullptr) by propagate are now treated as one entity by bundling them
        // into one proxy
        for (size_t i = 0; i != n; ++i) {
            if (abstr_vars[i]) continue;

            auto vars = DefVec();
            auto vi   = lam->tvar(i);
            auto ai   = abstr_args[i];
            vars.emplace_back(vi);

            for (size_t j = i + 1; j != n; ++j) {
                auto vj = lam->tvar(j);
                if (!abstr_vars[j] && abstr_args[j] == ai) vars.emplace_back(vj);
            }

            if (vars.size() == 1) {
                lattice_[vi] = abstr_vars[i] = vi; // top
            } else {
                auto proxy = world().proxy(vi->type(), vars, 0, PROXY_GVN);

                for (auto p : proxy->ops()) {
                    auto j       = get_index(p);
                    auto vj      = lam->tvar(j);
                    lattice_[vj] = abstr_vars[j] = proxy;
                }

                DLOG("bundle: {}", proxy);
            }
        }

        // GVN split: We have to prove that all incoming args for all vars in a bundle are the same value.
        // Otherwise we have to refine the bundle by splitting off contradictions.
        // E.g.: Say we started with `{a, b, c, d, e}` as a single bundle for all tvars of `lam`.
        // Now, we see `lam (x, y, x, y, z)`. Then we have to build:
        // a -> {a, c}
        // b -> {b, d}
        // c -> {a, c}
        // d -> {b, d}
        // e -> e      (top)
        for (size_t i = 0; i != n; ++i) {
            if (auto proxy = isa_gvn_proxy(abstr_vars[i])) {
                auto num  = proxy->num_ops();
                auto vars = DefVec();
                auto ai   = abstr_args[i];

                for (auto p : proxy->ops()) {
                    auto j  = get_index(p);
                    auto vj = lam->tvar(j);
                    if (p == vj) {
                        if (ai == abstr_args[j]) vars.emplace_back(vj);
                    }
                }

                auto new_num = vars.size();
                if (new_num == 1) {
                    ELOG("setting todo due to new gvn slit");
                    todo_        = true;
                    auto vi      = lam->tvar(i);
                    lattice_[vi] = abstr_vars[i] = vi;
                    DLOG("single: {}", vi);
                } else if (new_num != num) {
                    ELOG("setting todo due to new gvn slit");
                    todo_          = true;
                    auto new_proxy = world().proxy(ai->type(), vars, 0, PROXY_GVN);
                    DLOG("split: {}", new_proxy);

                    for (auto p : new_proxy->ops()) {
                        auto j  = get_index(p);
                        auto vj = lam->tvar(j);
                        if (p == vj) lattice_[vj] = abstr_vars[j] = new_proxy;
                    }
                }
                // if new_num == num: do nothing
            }
        }

        // set new abstract var
        auto abstr_var = world().tuple(abstr_vars);
        map(lam->var(), abstr_var);
        lattice_[lam->var()] = abstr_var;

        if (!lookup(lam))
            for (auto d : lam->deps())
                rewrite(d);

        return world().app(lam, abstr_args);
    }

    return mim::Analysis::rewrite_imm_App(app);
}

static bool keep(const Def* old_var, const Def* abstr) {
    if (old_var == abstr) return true; // top
    auto proxy = isa_gvn_proxy(abstr);
    return proxy && proxy->op(0) == old_var; // first in GVN bundle?
}

const Def* SymExprOpt::rewrite_imm_App(const App* old_app) {
    if (auto old_lam = old_app->callee()->isa_mut<Lam>()) {
        if (auto l = lattice(old_lam->var()); l && l != old_lam->var()) {
            todo_ = true;

            size_t num_old = old_lam->num_tvars();
            Lam* new_lam;
            if (auto i = lam2lam_.find(old_lam); i != lam2lam_.end())
                new_lam = i->second;
            else {
                // build new dom
                auto new_doms = DefVec();
                for (size_t i = 0; i != num_old; ++i) {
                    auto old_var = old_lam->var(num_old, i);
                    auto abstr   = lattice(old_var);
                    if (keep(old_var, abstr)) new_doms.emplace_back(rewrite(old_lam->dom(num_old, i)));
                }

                // build new lam
                size_t num_new    = new_doms.size();
                auto new_vars     = absl::FixedArray<const Def*>(num_old);
                new_lam           = new_world().mut_lam(new_doms, rewrite(old_lam->codom()))->set(old_lam->dbg());
                lam2lam_[old_lam] = new_lam;

                // build new var
                for (size_t i = 0, j = 0; i != num_old; ++i) {
                    auto old_var = old_lam->var(num_old, i);
                    auto abstr   = lattice(old_var);

                    if (keep(old_var, abstr)) {
                        auto v      = new_lam->var(num_new, j++);
                        new_vars[i] = v;
                        if (abstr != old_var) map(abstr, v); // GVN bundle
                    } else {
                        new_vars[i] = rewrite(abstr); // SCCP propagate
                    }
                }

                map(old_lam->var(), new_vars);
                new_lam->set(rewrite(old_lam->filter()), rewrite(old_lam->body()));
            }

            // build new app
            size_t num_new = new_lam->num_vars();
            auto new_args  = absl::FixedArray<const Def*>(num_new);
            for (size_t i = 0, j = 0; i != num_old; ++i) {
                auto old_var = old_lam->var(num_old, i);
                auto abstr   = lattice(old_var);
                if (keep(old_var, abstr)) new_args[j++] = rewrite(old_app->targ(i));
            }

            return map(old_app, new_world().app(new_lam, new_args));
        }
    }

    return Rewriter::rewrite_imm_App(old_app);
}

} // namespace mim::plug::mem::phase
