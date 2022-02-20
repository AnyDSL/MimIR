
#include "thorin/pass/rw/closure2sjlj.h"
#include "thorin/transform/closure_conv.h"

namespace thorin {

static void get_exn_closures(LamMap<std::pair<int, const Def*>>& out, const Def* def) {
    if (def->no_dep() || def->level() != Sort::Term || def->isa_nom<Lam>()) return;
    if (auto c = isa_closure_lit(def)) {
        if (c.is_basicblock() && !out.contains(c.fnc_as_lam()))
            out[c.fnc_as_lam()] = {out.size() + 1, c.env()};
        get_exn_closures(out, c.env());
    } else {
        for (auto op: def->ops())
            get_exn_closures(out, op);
    }
}

void Closure2SjLj::get_exn_closures() {
    lam2tag_.clear();
    if (!ignore(curr_nom()) || !curr_nom()->type()->is_cn()) return;
    if (auto app = curr_nom()->body()->isa<App>())
        thorin::get_exn_closures(lam2tag_, curr_nom()->body()->as<App>()->arg());
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

// Note: closure dom
Lam* Closure2SjLj::get_throw(const Def* dom) {
    auto& w = world();
    auto [p, inserted] = dom2throw_.emplace(dom, nullptr);
    auto& thw = p->second;
    if (inserted || !thw) {
        auto pi = w.cn(closure_sub_env(dom, w.tuple({jb_type(), rb_type(), tag_type()})));
        auto tlam = w.nom_lam(pi, w.dbg("throw"));
        auto [m0, env, var] = split(tlam->var());
        auto [jbuf, rbuf, tag] = env->projs<3>();
        auto [m1, r] = w.op_alloc(var->type(), m0)->projs<2>();
        auto m2 = w.op_store(m1, r, var);
        r = w.op_bitcast(w.type_ptr(w.type_ptr(var->type())), rbuf);
        auto m3 = w.op_store(m2, rbuf, r);
        tlam->set_body(w.op_longjmp(m3, jbuf, tag));
        tlam->set_filter(w.lit_false());
    }
    return thw;
}

Lam* Closure2SjLj::get_lpad(Lam* lam) {
    auto& w = world();
    auto [p, inserted] = lam2lpad_.emplace(lam, nullptr);
    auto& lpad = p->second;
    if (inserted || !lpad) {
        auto [_, env_type, dom] = split(lam->dom());
        auto pi = w.cn_mem(w.tuple({env_type, void_ptr()}));
        auto llam = w.nom_lam(pi, w.dbg("lpad"));
        auto [m, env, arg_ptr] = split(llam->var());
        arg_ptr = w.op_bitcast(w.type_ptr(dom), arg_ptr);
        auto [m2, args] = w.op_load(m, arg_ptr)->projs<2>();
        llam->app(lam, rebuild(m2, env, args));
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

        auto new_args = w.tuple(curr_nom()->vars())->refine(0, m2);
        curr_nom()->set(curr_nom()->apply(new_args));

        cur_jbuf_ = jb;
        cur_rbuf_ = rb;
    }

    auto body = curr_nom()->body()->as<App>();

    auto branches = DefVec(lam2tag_.size() + 1);
    {
        auto env = w.tuple(body->args().skip_front());
        auto new_callee = w.nom_lam(w.cn_mem(w.tuple({env->type(), void_ptr()})), w.dbg("sjlj_wrap"));
        auto [m, env_var, _] = split(new_callee->var());
        auto new_args = DefArray(env->num_ops(), [&](auto i) {
            return (i == 0) ? m : env_var->proj(i - 1);
        });
        new_callee->app(body->callee(), new_args, body->dbg());
        branches[0] = pack_closure(env, new_callee);
    }

    for (auto [exn_lam, p]: lam2tag_) {
        auto [i, env] = p;
        branches[i] = pack_closure(env, get_lpad(exn_lam));
    }

    auto m0 = body->arg(0); 
    assert(m0->type() == w.type_mem());
    auto [m1, tag] = w.op_setjmp(m0, cur_jbuf_)->projs<2>();
    auto [m2, rb] = w.op_load(m1, cur_rbuf_)->projs<2>();
    tag = w.op(Conv::s2s, w.type_int_width(lam2tag_.size()), tag);
    auto br = w.extract(w.tuple(branches), tag);
    curr_nom()->app(br, {m2, rb});
}

const Def* Closure2SjLj::rewrite(const Def* def) {
    if (auto c = isa_closure_lit(def); c && lam2tag_.contains(c.fnc_as_lam())) {
        auto& w = world();
        auto [i, _] = lam2tag_[c.fnc_as_lam()];
        auto tlam = get_throw(c.fnc_as_lam()->dom());
        return pack_closure(w.tuple({cur_jbuf_, cur_rbuf_, w.lit_int(i)}), tlam);
    }
    return def;
}

}
