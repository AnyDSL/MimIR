#include "dialects/clos/phase/lower_typed_clos.h"

#include <functional>

#include "thorin/check.h"

namespace thorin::clos {

namespace {
const Def* insert_ret(const Def* def, const Def* ret) {
    auto new_ops = vector<const Def*>(def->num_projs() + 1,
                                      [&](auto i) { return (i == def->num_projs()) ? ret : def->proj(i); });
    auto& w      = def->world();
    return def->is_term() ? w.tuple(new_ops) : w.sigma(new_ops);
}
} // namespace

void LowerTypedClos::start() {
    // TODO put into c'tor - doesn't work right now, because world becomes invalid
    dummy_ret_ = world().bot(world().cn(world().annex<mem::M>()));

    auto externals = std::vector(world().externals().begin(), world().externals().end());
    for (auto [_, n] : externals) rewrite(n);
    while (!worklist_.empty()) {
        auto [lvm, lcm, lam] = worklist_.front();
        worklist_.pop();
        lcm_ = lcm;
        lvm_ = lvm;
        world().DLOG("in {} (lvm={}, lcm={})", lam, lvm_, lcm_);
        if (lam->is_set()) {
            auto new_f = rewrite(lam->filter());
            auto new_b = rewrite(lam->body());
            lam->reset({new_f, new_b});
        }
    }
}

Lam* LowerTypedClos::make_stub(Lam* lam, enum Mode mode, bool adjust_bb_type) {
    assert(lam && "make_stub: not a lam");
    if (auto i = old2new_.find(lam); i != old2new_.end() && i->second->isa_mut<Lam>()) return i->second->as_mut<Lam>();
    auto& w      = world();
    auto new_dom = w.sigma(vector<const Def*>(lam->num_doms(), [&](auto i) -> const Def* {
        auto new_dom = rewrite(lam->dom(i));
        if (i == Clos_Env_Param) {
            if (mode == Unbox)
                return env_type();
            else if (mode == Box)
                return world().call<mem::Ptr0>(new_dom);
        }
        return new_dom;
    }));
    if (Lam::isa_basicblock(lam) && adjust_bb_type) new_dom = insert_ret(new_dom, dummy_ret_->type());
    auto new_type = w.cn(new_dom);
    auto new_lam  = lam->stub(w, new_type);
    w.DLOG("stub {} ~> {}", lam, new_lam);
    if (lam->is_set()) new_lam->set(lam->filter(), lam->body());
    if (lam->is_external()) lam->transfer_external(new_lam);
    const Def* lcm = mem::mem_var(new_lam);
    const Def* env
        = new_lam->var(Clos_Env_Param); //, (mode != No_Env) ? w.dbg("closure_env") : lam->var(Clos_Env_Param)->dbg());
    if (mode == Box) {
        auto env_mem = w.call<mem::load>(Defs{lcm, env});
        lcm          = w.extract(env_mem, 0_u64)->set("mem");
        env          = w.extract(env_mem, 1_u64)->set("closure_env");
    } else if (mode == Unbox) {
        env = w.call<core::bitcast>(lam->dom(Clos_Env_Param), env)->set("unboxed_env");
    }
    auto new_args = w.tuple(vector<const Def*>(lam->num_doms(), [&](auto i) {
        return (i == Clos_Env_Param) ? env : (lam->var(i) == mem::mem_var(lam)) ? lcm : *new_lam->var(i);
    }));
    assert(new_args->num_projs() == lam->num_doms());
    assert(lam->num_doms() <= new_lam->num_doms());
    map(lam->var(), new_args);
    worklist_.emplace(mem::mem_var(lam), lcm, new_lam);
    return map(lam, new_lam)->as<Lam>();
}

const Def* LowerTypedClos::rewrite(const Def* def) {
    switch (def->node()) {
        case Node::Bot:
        case Node::Top:
        case Node::Type:
        case Node::Univ:
        case Node::Nat: return def;
    }

    auto& w = world();

    if (auto i = old2new_.find(def); i != old2new_.end()) return i->second;
    if (auto var = def->isa<Var>();
        var && var->mut()->isa_mut<Lam>()) // TODO put this conditions inside the assert below
        assert(false && "Lam vars should appear in a map!");

    auto new_type = rewrite(def->type());

    if (auto ct = isa_clos_type(def)) {
        auto pi = rewrite(ct->op(1))->as<Pi>();
        if (Pi::isa_basicblock(pi)) pi = w.cn(insert_ret(pi->dom(), dummy_ret_->type()));
        auto env_type = rewrite(ct->op(2));
        return map(def, w.sigma({pi, env_type}));
    } else if (auto proj = def->isa<Extract>()) {
        auto tuple = proj->tuple();
        auto idx   = Lit::isa(proj->index());
        if (isa_clos_type(tuple->type())) {
            assert(idx && idx <= 2 && "unknown proj from closure tuple");
            if (*idx == 0)
                return map(def, env_type());
            else
                return map(def, rewrite(tuple)->proj(*idx - 1));
        } else if (auto var = tuple->isa<Var>(); var && isa_clos_type(var->mut())) {
            assert(false && "proj fst type form closure type");
        }
    }

    if (auto c = isa_clos_lit(def)) {
        auto env      = rewrite(c.env());
        auto mode     = (env->type()->isa<Idx>() || match<mem::Ptr>(env->type())) ? Unbox : Box;
        const Def* fn = make_stub(c.fnc_as_lam(), mode, true);
        if (env->type() == w.sigma()) {
            // Optimize empty env
            env = w.bot(env_type());
        } else if (!mode) {
            auto mem_ptr = (c.get() == attr::esc) ? mem::op_alloc(env->type(), lcm_) : mem::op_slot(env->type(), lcm_);
            auto mem     = w.extract(mem_ptr, 0_u64);
            auto env_ptr = mem_ptr->proj(1_u64); //, w.dbg(fn->sym() + "_env"));
            lcm_         = w.call<mem::store>(Defs{mem, env_ptr, env});
            map(lvm_, lcm_);
            env = env_ptr;
        }
        fn  = w.call<core::bitcast>(new_type->op(0), fn);
        env = w.call<core::bitcast>(new_type->op(1), env);
        return map(def, w.tuple({fn, env}));
    } else if (auto lam = def->isa_mut<Lam>()) {
        return make_stub(lam, No_Env, false);
    } else if (auto mut = def->isa_mut()) {
        assert(!isa_clos_type(mut));
        auto new_mut = mut->stub(w, new_type);
        map(mut, new_mut);
        for (size_t i = 0; i < mut->num_ops(); i++)
            if (mut->op(i)) new_mut->set(i, rewrite(mut->op(i)));
        if (!def->isa_mut<Global>() && Check::alpha(mut, new_mut)) return map(mut, mut);
        if (auto imm = new_mut->immutabilize()) return map(mut, imm);
        return new_mut;
    } else if (def->isa<Axiom>()) {
        return def;
    } else {
        auto new_ops = vector<const Def*>(def->num_ops(), [&](auto i) { return rewrite(def->op(i)); });

        if (auto app = def->isa<App>()) {
            // Add dummy retcont to first-class BB
            if (auto p = app->callee()->isa<Extract>();
                p && isa_clos_type(p->tuple()->type()) && Pi::isa_basicblock(app->callee_type()))
                new_ops[1] = insert_ret(new_ops[1], dummy_ret_);
        }

        auto new_def = def->rebuild(w, new_type, new_ops);

        // We may need to update the mem token after all ops have been rewritten:
        // F (m, a1, ..., (env, f):pct)
        // ~>
        // let [m', env_ptr] = :alloc T m'
        // let m'' = :store env_ptr env
        // F (m, a1', ..., (env_ptr, f'))
        // ~>
        // let ...
        // F (m'', a1', ..., (env_ptr, f'))
        for (size_t i = 0; i < new_def->num_ops(); i++)
            if (new_def->op(i)->type() == w.annex<mem::M>()) new_def = new_def->refine(i, lcm_);

        if (new_type == w.annex<mem::M>()) { // :store
            lcm_ = new_def;
            lvm_ = def;
        } else if (new_type->isa<Sigma>()) { // :alloc, :slot, ...
            for (size_t i = 0; i < new_type->num_ops(); i++) {
                if (new_type->op(i) == w.annex<mem::M>()) {
                    lcm_ = w.extract(new_def, i);
                    lvm_ = w.extract(def, i);
                    break;
                }
            }
        }

        return map(def, new_def);
    }
}

} // namespace thorin::clos
