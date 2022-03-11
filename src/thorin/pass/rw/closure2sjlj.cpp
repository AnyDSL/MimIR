
#include "thorin/pass/rw/closure2sjlj.h"
#include "thorin/transform/closure_conv.h"

namespace thorin {

void Closure2SjLj::get_exn_closures(const Def* def, DefSet& visited) {
    if (def->level() != Sort::Term || def->isa_nom<Lam>() || visited.contains(def)) return;
    visited.emplace(def);
    if (auto c = isa_closure_lit(def)) {
        auto lam = c.fnc_as_lam();
        if (c.is_basicblock() && !ignore_.contains(lam)) {
            def->world().DLOG("FOUND exn closure: {}", c.fnc_as_lam());
            lam2tag_[c.fnc_as_lam()] = {lam2tag_.size() + 1, c.env()};
        }
        get_exn_closures(c.env(), visited);
    } else {
        for (auto op: def->ops())
            get_exn_closures(op, visited);
    }
}

void Closure2SjLj::get_exn_closures() {
    lam2tag_.clear();
    if (!curr_nom()->is_set() || !curr_nom()->type()->is_cn()) return;
    auto app = curr_nom()->body()->isa<App>();
    if (!app) return;
    if (auto p = app->callee()->isa<Extract>(); p && isa_ctype(p->tuple()->type())) {
        auto p2 = p->tuple()->isa<Extract>();
        if (p2 && p2->tuple()->isa<Tuple>()) {
            // branch: Check the closure environments, but be careful not to traverse
            // the closures themselves
            auto branches = p2->tuple()->ops();
            for (auto b: branches) {
                auto c = isa_closure_lit(b);
                if (c) {
                    ignore_.emplace(c.fnc_as_lam());
                    world().DLOG("IGNORE {}", c.fnc_as_lam());
                }
            }
        }
    }
    auto visited = DefSet();
    get_exn_closures(app->arg(), visited);
}

static std::array<const Def*, 3> split(const Def* def) {
    auto new_ops = DefArray(def->num_projs() - 2, nullptr);
    auto& w = def->world();
    const Def* mem, *env;
    auto j = 0;
    for (size_t i = 0; i < def->num_projs(); i++) {
        auto op = def->proj(i);
        if (op == w.type_mem() || op->type() == w.type_mem())
            mem = op;
        else if (i == CLOSURE_ENV_PARAM)
            env = op;
        else
            new_ops[j++] = op;
    }
    assert(mem && env);
    auto remaining = (def->level() == Sort::Term) ? w.tuple(new_ops) : w.sigma(new_ops);
    return {mem, env, remaining};
}

static const Def* rebuild(const Def* mem, const Def* env, const Def* remaining) {
    auto& w = mem->world();
    auto env_seen = false;
    auto new_ops = DefArray(remaining->num_projs() + 2, [&](auto i) {
        if (i == 0) return mem;
        if (i == 1) { env_seen = true; return env; }
        return remaining->proj(env_seen ? i - 1 : i - 2);
    });
    return (remaining->level() == Sort::Term) ? w.tuple(new_ops) : w.sigma(new_ops);
}

Lam* Closure2SjLj::get_throw(const Def* dom) {
    auto& w = world();
    auto [p, inserted] = dom2throw_.emplace(dom, nullptr);
    auto& tlam = p->second;
    if (inserted || !tlam) {
        auto pi = w.cn(closure_sub_env(dom, w.sigma({jb_type(), rb_type(), tag_type()})));
        tlam = w.nom_lam(pi, w.dbg("throw"));
        auto [m0, env, var] = split(tlam->var());
        auto [jbuf, rbuf, tag] = env->projs<3>();
        auto [m1, r] = w.op_alloc(var->type(), m0)->projs<2>();
        auto m2 = w.op_store(m1, r, var);
        rbuf = w.op_bitcast(w.type_ptr(w.type_ptr(var->type())), rbuf);
        auto m3 = w.op_store(m2, rbuf, r);
        tlam->set_body(w.op_longjmp(m3, jbuf, tag));
        tlam->set_filter(w.lit_false());
        ignore_.emplace(tlam);
    }
    return tlam;
}

Lam* Closure2SjLj::get_lpad(Lam* lam, const Def* rb) {
    auto& w = world();
    auto [p, inserted] = lam2lpad_.emplace(w.tuple({lam, rb}), nullptr);
    auto& lpad = p->second;
    if (inserted || !lpad) {
        auto [_, env_type, dom] = split(lam->dom());
        auto pi = w.cn(w.sigma({w.type_mem(), env_type}));
        lpad = w.nom_lam(pi, w.dbg("lpad"));
        auto [m, env, __] = split(lpad->var());
        auto [m1, arg_ptr] = w.op_load(m, rb)->projs<2>();
        arg_ptr = w.op_bitcast(w.type_ptr(dom), arg_ptr);
        auto [m2, args] = w.op_load(m1, arg_ptr)->projs<2>();
        lpad->app(lam, rebuild(m2, env, args));
        ignore_.emplace(lpad);
    }
    return lpad;
}

void Closure2SjLj::enter() {
    auto&  w = world();
    get_exn_closures();
    if (lam2tag_.empty()) return;

    {
        auto m0 = curr_nom()->mem_var();
        auto [m1, jb] = w.op_alloc_jumpbuf(m0)->projs<2>();
        auto [m2, rb] = w.op_slot(void_ptr(), m1)->projs<2>();

        auto new_args = curr_nom()->vars();
        new_args[0] = m2;
        curr_nom()->set(curr_nom()->apply(w.tuple(new_args)));

        cur_jbuf_ = jb;
        cur_rbuf_ = rb;

        // apparently apply() can change the id of the closures, so we have to do it again :(
        get_exn_closures();
    }

    auto body = curr_nom()->body()->as<App>();

    auto branch_type = ctype(w.cn(w.type_mem()));
    auto branches = DefVec(lam2tag_.size() + 1);
    {
        auto env = w.tuple(body->args().skip_front());
        auto new_callee = w.nom_lam(w.cn({w.type_mem(), env->type()}), w.dbg("sjlj_wrap"));
        auto [m, env_var, _] = split(new_callee->var());
        auto new_args = DefArray(env->num_ops() + 1, [&](auto i) {
            return (i == 0) ? m : env_var->proj(i - 1);
        });
        new_callee->app(body->callee(), new_args, body->dbg());
        branches[0] = pack_closure(env, new_callee, branch_type);
    }

    for (auto [exn_lam, p]: lam2tag_) {
        auto [i, env] = p;
        branches[i] = pack_closure(env, get_lpad(exn_lam, cur_rbuf_), branch_type);
    }

    auto m0 = body->arg(0); 
    assert(m0->type() == w.type_mem());
    auto [m1, tag] = w.op_setjmp(m0, cur_jbuf_)->projs<2>();
    tag = w.op(Conv::s2s, w.type_int(branches.size()), tag);
    auto branch = w.extract(w.tuple(branches), tag);
    curr_nom()->set_body(apply_closure(branch, m1));
}

const Def* Closure2SjLj::rewrite(const Def* def) {
    if (auto c = isa_closure_lit(def); c && lam2tag_.contains(c.fnc_as_lam())) {
        auto& w = world();
        auto [i, _] = lam2tag_[c.fnc_as_lam()];
        auto tlam = get_throw(c.fnc_as_lam()->dom());
        return pack_closure(w.tuple({cur_jbuf_, cur_rbuf_, w.lit_int(i)}), tlam, c.type());
    }
    return def;
}

}
