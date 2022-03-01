#include <functional>

#include "thorin/check.h"
#include "thorin/transform/lower_typed_closures.h"

namespace thorin {

void LowerTypedClosures::run() {
    auto externals = std::vector(world().externals().begin(), world().externals().end());
    for (auto [_, n]: externals)
        rewrite(n);
    while (!worklist_.empty()) {
        auto [lvm, lcm, lam] = worklist_.front();
        worklist_.pop();
        lcm_ = lcm;
        lvm_ = lvm;
        world().DLOG("in {} (lvm={}, lcm={})", lam, lvm_, lcm_);
        if (lam->is_set()) {
            lam->set_body(rewrite(lam->body()));
            lam->set_filter(rewrite(lam->filter()));
        }
    }
}

static const Def* insert_ret(const Def* def, const Def* ret) {
    auto new_ops = DefArray(def->num_projs() + 1, [&](auto i) {
        return (i == def->num_projs()) ? ret : def->proj(i);
    });
    auto& w = def->world();
    return (def->level() == Sort::Term) ? w.tuple(new_ops) : w.sigma(new_ops);
}

Lam *LowerTypedClosures::make_stub(Lam* lam, enum Mode mode, bool adjust_bb_type) {
    assert(lam && "make_stub: not a lam");
    if (auto new_lam = old2new_.lookup(lam); new_lam && (*new_lam)->isa_nom<Lam>())
        return (*new_lam)->as_nom<Lam>();
    auto& w = world();
    auto new_dom = w.sigma(Array<const Def*>(lam->num_doms(), [&](auto i) -> const Def* {
        auto new_dom = rewrite(lam->dom(i));
        if (i == CLOSURE_ENV_PARAM) {
            if (mode == Unbox)    return env_type();
            else if (mode == Box) return w.type_ptr(new_dom);
        }
        return new_dom;
    }));
    if (lam->is_basicblock() && adjust_bb_type)
        new_dom = insert_ret(new_dom, dummy_ret_->type());
    auto new_type = w.cn(new_dom);
    auto new_lam = lam->stub(w, new_type, w.dbg(lam->name()));
    w.DLOG("stub {} ~> {}", lam, new_lam);
    new_lam->set_name(lam->name());
    new_lam->set_body(lam->body());
    new_lam->set_filter(lam->filter());
    if (lam->is_external()) {
        lam->make_internal();
        new_lam->make_external();
    }
    const Def* lcm = new_lam->mem_var();
    const Def* env = new_lam->var(CLOSURE_ENV_PARAM, w.dbg("closure_env"));
    if (mode == Box) {
        auto env_mem = w.op_load(lcm, env);
        lcm = w.extract(env_mem, 0_u64, w.dbg("mem"));
        env = w.extract(env_mem, 1_u64, w.dbg("closure_env"));
    } else if (mode == Unbox) {
        env = w.op_bitcast(lam->dom(CLOSURE_ENV_PARAM), env, w.dbg("unboxed_env"));
    }
    auto new_args = w.tuple(Array<const Def*>(lam->num_doms(), [&](auto i) {
        return (i == CLOSURE_ENV_PARAM) ? env
             : (lam->var(i) == lam->mem_var()) ? lcm
             : new_lam->var(i);
    }));
    assert(new_args->num_projs() == lam->num_doms());
    assert(lam->num_doms() <= new_lam->num_doms());
    map(lam->var(), new_args);
    worklist_.emplace(lam->mem_var(), lcm, new_lam);
    return map<Lam>(lam, new_lam);
}

const Def* LowerTypedClosures::rewrite(const Def* def) {
    switch(def->node()) {
        case Node::Bot:
        case Node::Top:
        case Node::Kind:
        case Node::Space:
        case Node::Nat:
            return def;
    }

    auto& w = world();

    if (auto new_def = old2new_.lookup(def))
        return *new_def;

    if (auto var = def->isa<Var>(); var && var->nom()->isa_nom<Lam>())
        assert(false && "Lam vars should appear in a map!");

    auto new_type = rewrite(def->type());
    auto new_dbg = def->dbg() ? rewrite(def->dbg()) : nullptr;

    if (auto ct = isa_ctype(def)) {
        auto pi = rewrite(ct->op(1))->as<Pi>();
        if (pi->is_basicblock())
            pi = w.cn(insert_ret(pi->dom(), dummy_ret_->type()));
        auto env_type = rewrite(ct->op(2));
        return map(def, w.sigma({pi, env_type}));
    } else if (auto proj = def->isa<Extract>()) {
        auto tuple = proj->tuple();
        auto idx = isa_lit(proj->index());
        if (isa_ctype(tuple->type())) {
            assert (idx && idx <= 2 && "unknown proj from closure tuple");
            if (*idx == 0)
                return map(def, env_type());
            else
                return map(def, rewrite(tuple)->proj(*idx - 1));
        } else if (auto var = tuple->isa<Var>(); var && isa_ctype(var->nom())) {
            assert(false && "proj fst type form closure type");
        }
    }

    if (auto c = isa_closure_lit(def)) {
        auto env = rewrite(c.env());
        auto mode = isa<Tag::Int>(env->type()) ? Unbox : Box;
        const Def* fn = make_stub(c.fnc_as_lam(), mode, true);
        if (env->type() == w.sigma()) {
            // Optimize empty env
            env = w.bot(env_type());
        } else if (!mode) {
            auto mem_ptr = (c.mark() == CConv::escaping) 
                ? w.op_alloc(env->type(), lcm_)
                : w.op_slot(env->type(), lcm_);
            auto mem = w.extract(mem_ptr, 0_u64);
            auto env_ptr = mem_ptr->proj(1_u64, w.dbg(fn->name() + "_env"));
            lcm_ = w.op_store(mem, env_ptr, env);
            map(lvm_, lcm_);
            env = env_ptr;
        }
        fn = w.op_bitcast(new_type->op(0), fn);
        env = w.op_bitcast(new_type->op(1), env);
        return map(def, w.tuple({fn, env}));
    } else if (auto lam = def->isa_nom<Lam>()) {
        // Internal Lam's in callee pos are scalarized, i.e their 2nd param is not really the environment
        auto mode = (lam->is_internal() && lam->is_set()) ? No_Env : Box;
        return make_stub(lam, mode, false);
    } else if (auto nom = def->isa_nom()) {
        assert(!isa_ctype(nom));
        auto new_nom = nom->stub(w, new_type, new_dbg);
        map(nom, new_nom);
        for (size_t i = 0; i < nom->num_ops(); i++)
            if (nom->op(i))
                new_nom->set(i, rewrite(nom->op(i)));
        if (Checker(w).equiv(nom, new_nom))
            return map(nom, nom);
        if (auto restruct = new_nom->restructure())
            return map(nom, restruct);
        return new_nom;
    } else {
        auto new_ops = Array<const Def*>(def->num_ops(), [&](auto i) {
            return rewrite(def->op(i));
        });

        if (auto app = def->isa<App>()) {
            // Add dummy retcont to first-class BB
            if (auto p = app->callee()->isa<Extract>(); p && isa_ctype(p->tuple()->type())
                    && app->callee_type()->is_basicblock())
                new_ops[1] = insert_ret(new_ops[1], dummy_ret_);

            // Change env of externals from [] to []*
            if (auto lam = app->callee()->isa_nom<Lam>(); lam && (!lam->is_set() || lam->is_external()))
                new_ops[1] = new_ops[1]->refine(CLOSURE_ENV_PARAM, w.bot(env_type()));
        }
        
        auto new_def = def->rebuild(w, new_type, new_ops, new_dbg);

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
            if (new_def->op(i)->type() == w.type_mem())
                new_def = new_def->refine(i, lcm_);

        if (new_type == w.type_mem()) { // :store
            lcm_ = new_def;
            lvm_ = def;
        } else if (new_type->isa<Sigma>()) {  // :alloc, :slot, ...
            for (size_t i = 0; i < new_type->num_ops(); i++) {
                if (new_type->op(i) == w.type_mem()) {
                    lcm_ = w.extract(new_def, i);
                    lvm_ = w.extract(def, i);
                    break;
                }
            }
        }

        return map(def, new_def);
    }
}

}
