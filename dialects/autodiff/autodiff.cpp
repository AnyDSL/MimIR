#include "dialects/autodiff/autodiff.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>
#include <thorin/pass/pipelinebuilder.h>

#include "dialects/autodiff/passes/autodiff_eval.h"
#include "dialects/autodiff/passes/autodiff_zero.h"
#include "dialects/autodiff/passes/autodiff_zero_cleanup.h"
#include "dialects/compile/passes/internal_cleanup.h"
#include "dialects/mem/mem.h"

using namespace std::literals;
using namespace thorin;

extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"autodiff",
            [](Passes& passes) {
                register_pass<autodiff::ad_eval_pass, autodiff::AutoDiffEval>(passes);
                register_pass<autodiff::ad_zero_pass, autodiff::AutoDiffZero>(passes);
                register_pass<autodiff::ad_zero_cleanup_pass, autodiff::AutoDiffZeroCleanup>(passes);
                register_pass<autodiff::ad_ext_cleanup_pass, compile::InternalCleanup>(passes, "internal_diff_");
            },
            nullptr, [](Normalizers& normalizers) { autodiff::register_normalizers(normalizers); }};
}

namespace thorin::autodiff {

const Def* id_pullback(const Def* A) {
    auto& world       = A->world();
    auto arg_pb_ty    = pullback_type(A, A);
    auto id_pb        = world.mut_lam(arg_pb_ty)->set("id_pb");
    auto id_pb_scalar = id_pb->var(0_s)->set("s");
    id_pb->app(true,
               id_pb->var(1), // can not use ret_var as the result might be higher order
               id_pb_scalar);

    return id_pb;
}

const Def* zero_pullback(const Def* E, const Def* A) {
    auto& world    = A->world();
    auto A_tangent = tangent_type_fun(A);
    auto pb_ty     = pullback_type(E, A);
    auto pb        = world.mut_lam(pb_ty)->set("zero_pb");
    world.DLOG("zero_pullback for {} resp. {} (-> {})", E, A, A_tangent);
    pb->app(true, pb->var(1), op_zero(A_tangent));
    return pb;
}

//  `P` => `P*`
//  TODO: nothing? function => R? Mem => R?
//  TODO: rename to op_tangent_type
const Def* tangent_type_fun(const Def* ty) { return ty; }

/// computes pb type `E* -> A*`
/// `E` - type of the expression (return type for a function)
/// `A` - type of the argument (point of orientation resp. derivative - argument type for partial pullbacks)
const Pi* pullback_type(const Def* E, const Def* A) {
    auto& world   = E->world();
    auto tang_arg = tangent_type_fun(A);
    auto tang_ret = tangent_type_fun(E);
    auto pb_ty    = world.cn({tang_ret, world.cn(tang_arg)});
    return pb_ty;
}

// `A,R` => `(A->R)' = A' -> R' * (R* -> A*)`
const Pi* autodiff_type_fun(const Def* arg, const Def* ret) {
    auto& world = arg->world();
    world.DLOG("autodiff type for {} => {}", arg, ret);
    auto aug_arg = autodiff_type_fun(arg);
    auto aug_ret = autodiff_type_fun(ret);
    world.DLOG("augmented types: {} => {}", aug_arg, aug_ret);
    if (!aug_arg || !aug_ret) return nullptr;
    // `Q* -> P*`
    auto pb_ty = pullback_type(ret, arg);
    world.DLOG("pb type: {}", pb_ty);
    // `P' -> Q' * (Q* -> P*)`

    auto deriv_ty = world.cn({aug_arg, world.cn({aug_ret, pb_ty})});
    world.DLOG("autodiff type: {}", deriv_ty);
    return deriv_ty;
}

const Pi* autodiff_type_fun_pi(const Pi* pi) {
    auto& world = pi->world();
    if (!pi->is_cn()) {
        // TODO: dependency
        auto arg = pi->dom();
        auto ret = pi->codom();
        if (ret->isa<Pi>()) {
            auto aug_arg = autodiff_type_fun(arg);
            if (!aug_arg) return nullptr;
            auto aug_ret = autodiff_type_fun(pi->codom());
            if (!aug_ret) return nullptr;
            return world.pi(aug_arg, aug_ret);
        }
        return autodiff_type_fun(arg, ret);
    }
    auto [arg, ret_pi] = pi->doms<2>();
    auto ret           = ret_pi->as<Pi>()->dom();
    world.DLOG("compute AD type for pi");
    return autodiff_type_fun(arg, ret);
}

// In general transforms `A` => `A'`.
// Especially `P->Q` => `P'->Q' * (Q* -> P*)`.
const Def* autodiff_type_fun(const Def* ty) {
    auto& world = ty->world();
    // TODO: handle DS (operators)
    if (auto pi = ty->isa<Pi>()) return autodiff_type_fun_pi(pi);
    // Also handles autodiff call from axiom declaration => abstract => leave it.
    world.DLOG("AutoDiff on type: {} <{}>", ty, ty->node_name());
    if (Idx::size(ty)) return ty;
    if (ty == world.type_nat()) return ty;
    if (auto arr = ty->isa<Arr>()) {
        auto shape   = arr->shape();
        auto body    = arr->body();
        auto body_ad = autodiff_type_fun(body);
        if (!body_ad) return nullptr;
        return world.arr(shape, body_ad);
    }
    if (auto sig = ty->isa<Sigma>()) {
        // TODO: mut sigma
        DefArray ops(sig->ops(), [&](const Def* op) { return autodiff_type_fun(op); });
        world.DLOG("ops: {,}", ops);
        return world.sigma(ops);
    }
    // mem
    if (match<mem::M>(ty)) return ty;
    world.WLOG("no-diff type: {}", ty);
    return nullptr;
}

const Def* zero_def(const Def* T) {
    // TODO: we want: zero mem -> zero mem or bot
    // zero [A,B,C] -> [zero A, zero B, zero C]
    auto& world = T->world();
    world.DLOG("zero_def for type {} <{}>", T, T->node_name());
    if (auto arr = T->isa<Arr>()) {
        auto shape      = arr->shape();
        auto body       = arr->body();
        auto inner_zero = world.app(world.ax<zero>(), body);
        auto zero_arr   = world.pack(shape, inner_zero);
        world.DLOG("zero_def for array of shape {} with type {}", shape, body);
        world.DLOG("zero_arr: {}", zero_arr);
        return zero_arr;
    } else if (Idx::size(T)) {
        // TODO: real
        auto zero = world.lit(T, 0)->set("zero");
        world.DLOG("zero_def for int is {}", zero);
        return zero;
    } else if (auto sig = T->isa<Sigma>()) {
        DefArray ops(sig->ops(), [&](const Def* op) { return world.app(world.ax<zero>(), op); });
        return world.tuple(ops);
    }

    // or return bot
    // or id => zero T
    // return world.app(world.ax<zero>(), T);
    return nullptr;
}

const Def* op_sum(const Def* T, DefArray defs) {
    // TODO: assert all are of type T
    auto& world = T->world();
    return world.app(world.app(world.ax<sum>(), {world.lit_nat(defs.size()), T}), defs);
}

} // namespace thorin::autodiff

namespace thorin {

bool is_continuation(const Def* e) { return e->type()->isa<Pi>() && e->type()->as<Pi>()->is_cn(); }

bool is_returning_continuation(const Def* e) {
    // TODO: fix open functions
    auto E = e->type();
    if (auto pi = E->isa<Pi>()) {
        if (pi->is_cn() && /* args, return */ pi->num_doms() == 2) {
            if (auto ret_pi = pi->dom(2, 1)->isa<Pi>())
                return ret_pi->is_cn() && ret_pi->is_cn();
        }
    }
    return false;
}

bool is_open_continuation(const Def* e) { return is_continuation(e) && !is_returning_continuation(e); }

/// The high level view is:
/// ```
/// f: B -> C
/// g: A -> B
/// f o g := λ x. f(g(x)) : A -> C
/// ```
/// In cps the types look like:
/// ```
/// f: cn[B, cn C]
/// g: cn[A, cn B]
/// h = f o g
/// h : cn[A, cn C]
/// h = λ a ret_h. g(a,h')
/// h' : cn B
/// h' = λ b. f(b,ret_h)
/// ```
const Def* compose_continuation(const Def* f, const Def* g) {
    assert(is_continuation(f));
    auto& world = f->world();
    world.DLOG("compose f (B->C): {} : {}", f, f->type());
    world.DLOG("compose g (A->B): {} : {}", g, g->type());
    assert(is_returning_continuation(f));
    assert(is_returning_continuation(g));

    auto F = f->type()->as<Pi>();
    auto G = g->type()->as<Pi>();

    auto A = G->cn_dom();
    auto B = G->cn_codom();
    auto C = F->cn_codom();
    // The type check of codom G = dom F is better handled by the application type checking

    world.DLOG("compose f (B->C): {} : {}", f, F);
    world.DLOG("compose g (A->B): {} : {}", g, G);
    world.DLOG("  A: {}", A);
    world.DLOG("  B: {}", B);
    world.DLOG("  C: {}", C);

    auto H     = world.cn({A, world.cn(C)});
    auto Hcont = world.cn(B);

    auto h     = world.mut_lam(H)->set("comp_"s + *f->sym() + "_"s + *g->sym());
    auto hcont = world.mut_lam(Hcont)->set("comp_"s + *f->sym() + "_"s + *g->sym() + "_cont"s);

    h->app(true, g, {h->var((nat_t)0), hcont});

    auto hcont_var = hcont->var(); // Warning: not var(0) => only one var => normalization flattens tuples down here.
    hcont->app(true, f, {hcont_var, h->var(1) /* ret_var */});

    return h;
}

} // namespace thorin
