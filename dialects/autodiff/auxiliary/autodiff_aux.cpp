#include "dialects/autodiff/auxiliary/autodiff_aux.h"

#include "thorin/def.h"
#include "thorin/tuple.h"

#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/builder.h"
#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

namespace thorin::autodiff {

const Def* id_pullback(const Def* A) {
    auto& world       = A->world();
    auto arg_pb_ty    = pullback_type(A, A);
    auto id_pb        = world.nom_lam(arg_pb_ty, world.dbg("id_pb"));
    auto id_pb_scalar = id_pb->var((nat_t)0, world.dbg("s"));
    // assert(id_pb!=NULL);
    // assert(id_pb->ret_var()!=NULL);
    // assert(id_pb_scalar!=NULL);
    id_pb->app(true,
               // id_pb->ret_var(),
               id_pb->var(1), // can not use ret_var as the result might be higher order
               id_pb_scalar);

    // world.DLOG("id_pb for type {} is {} : {}",A,id_pb,id_pb->type());
    return id_pb;
}

const Def* zero_pullback(const Def* E, const Def* A) {
    auto& world    = A->world();
    auto A_tangent = tangent_type_fun(A);
    auto pb_ty     = pullback_type(E, A);
    auto pb        = world.nom_lam(pb_ty, world.dbg("zero_pb"));
    world.DLOG("zero_pullback for {} resp. {} (-> {})", E, A, A_tangent);
    pb->app(true, pb->var(1), op_zero(A_tangent));
    return pb;
}

// R P => P'
// R TODO: function => extend
// R const Def* augment_type_fun(const Def* ty) { return ty; }
//  P => P*
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
    tang_arg->dump();
    tang_ret->dump();
    auto pb_ty = world.cn({equip_mem(tang_ret), world.cn(equip_mem(tang_arg))});
    pb_ty->dump();
    return pb_ty;
}

// `A,R => A'->R' * (R* -> A*) = (A->R)' `
const Pi* autodiff_type_fun(const Def* arg, const Def* ret) {
    auto& world = arg->world();
    world.DLOG("autodiff type for {} => {}", arg, ret);
    arg->dump();
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
    if (!is_continuation_type(pi)) {
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

// Performs the type transformation `A` => `A'`.
// This is of special importance for functions: `P->Q` => `P'->Q' * (Q* -> P*)`.
const Def* autodiff_type_fun(const Def* ty) {
    auto& world = ty->world();
    // TODO: handle DS (operators)
    if (auto pi = ty->isa<Pi>()) { return autodiff_type_fun_pi(pi); }
    // TODO: what is this object? (only numbers are printed)
    // possible abstract type from autodiff axiom
    world.DLOG("AutoDiff on type: {}", ty);

    if (auto mem = match<mem::M>(ty)) { return mem; }

    if (auto ptr = match<mem::Ptr>(ty)) {
        auto type = ptr->op(0);
        return ptr;
    }

    if (ty->isa<Idx>()) { return ty; }

    if (ty == world.type_nat()) return ty;
    if (auto arr = ty->isa<Arr>()) {
        auto shape   = arr->shape();
        auto body    = arr->body();
        auto body_ad = autodiff_type_fun(body);
        if (!body_ad) return nullptr;
        return world.arr(shape, body_ad);
    }
    if (auto sig = ty->isa<Sigma>()) {
        // TODO: nom sigma
        DefArray ops(sig->ops(), [&](const Def* op) { return autodiff_type_fun(op); });
        return world.sigma(ops);
    }

    world.WLOG("no-diff type: {}", ty);
    // ty->dump(300);
    // world.write("tmp.txt");
    // can not work with
    // assert(false && "autodiff_type should not be invoked on non-pi types");
    // return ty;
    return nullptr;
}

const Def* zero_def(const Def* T) {
    // TODO: we want: zero mem -> zero mem or bot
    // zero [A,B,C] -> [zero A, zero B, zero C]
    auto& world = T->world();
    world.DLOG("zero_def for type {} <{}>", T, T->node_name());
    if (auto arr = T->isa<Arr>()) {
        auto shape = arr->shape();
        auto body  = arr->body();
        // auto inner_zero = zero_def(body);
        auto inner_zero = op_zero(body);
        auto zero_arr   = world.pack(shape, inner_zero);
        world.DLOG("zero_def for array of shape {} with type {}", shape, body);
        world.DLOG("zero_arr: {}", zero_arr);
        return zero_arr;
        // }else if(auto lit = match<type_int_>()) { }
        // }else if(auto tint = T->isa<Tag::Int>()) {
    } else if (auto idx = T->isa<Idx>()) {
        // TODO: real
        auto zero = world.lit_idx(T, 0, world.dbg("zero"));
        world.DLOG("zero_def for int is {}", zero);
        return zero;
    } else if (auto app = T->isa<App>()) {
        auto callee = app->callee();
        // auto args = app->args();
        world.DLOG("app callee: {} : {} <{}>", callee, callee->type(), callee->node_name());
        // TODO: can you directly match Tag::Int?
        if (callee->isa<Idx>()) {
            // auto size = app->arg(0);
            auto zero = world.lit_idx(T, 0, world.dbg("zero"));
            // world.DLOG("zero_def for int of size {} is {}", size, zero);
            world.DLOG("zero_def for int is {}", zero);
            return zero;
        }
    } else if (auto sig = T->isa<Sigma>()) {
        DefArray ops(sig->ops(), [&](const Def* op) { return op_zero(op); });
        return world.tuple(ops);
    }

    if (match<mem::M>(T)) { return world.bot(mem::type_mem(world)); }

    if (match<mem::Ptr>(T)) {
        auto lit_zero = world.lit_int_(64, 0);
        return core::op_bitcast(T, lit_zero);
    }

    // or return bot
    // assert(0);
    // or id => zero T
    T->dump();
    return nullptr;
}

const Def* op_sum(const Def* T, DefArray defs) {
    // TODO: assert all are of type T
    auto& world = T->world();
    return world.raw_app(world.raw_app(world.ax<sum>(), {world.lit_nat(defs.size()), T}), world.tuple(defs));
}

} // namespace thorin::autodiff

namespace thorin {

const Pi* cn_mem_wrap(const Pi* pi) {
    auto& world = pi->world();
    auto dom    = pi->dom();

    const Pi* result;
    if (pi->ret_pi()) {
        auto arg    = equip_mem(dom->proj(0));
        auto ret_pi = cn_mem_wrap(dom->proj(1)->as<Pi>());
        result      = world.cn({arg, ret_pi});
    } else {
        auto arg = equip_mem(dom);
        result   = world.cn({arg});
    }

    return result;
}

const Def* lam_mem_wrap(const Def* lam) {
    auto& world = lam->world();
    auto type   = lam->type()->as<Pi>();
    if (!match<mem::M>(type->dom(0)->proj(0))) {
        auto wrap = cn_mem_wrap(type);

        auto mem_lam    = world.nom_lam(wrap, world.dbg("memorized_" + lam->name()));
        auto lam_return = world.nom_lam(type->ret_pi(), world.dbg("memorized_return_" + lam->name()));

        auto mem_vars = mem_lam->var((nat_t)0)->projs();
        auto mem      = mem_vars[0];
        auto vars     = lam_return->vars();

        auto compound = build(world).add(mem).add(vars).tuple();
        auto compound2 = build(world).add(mem_vars.skip_front()).add(lam_return).tuple();

        lam_return->set_body(world.app(mem_lam->ret_var(), compound));

        mem_lam->set_body(world.app(lam, compound2));

        mem_lam->set_filter(true);
        lam_return->set_filter(true);
        return mem_lam;
    }

    return lam;
}

bool contains_mem(const Def* def) {
    if (match<mem::M>(def)) { return true; }

    bool is_mem = false;
    if (def->isa<Sigma>()) {
        return std::ranges::any_of(def->ops(), [](auto op) { return contains_mem(op); });
    } else if (auto pack = def->isa<Pack>()) {
        return contains_mem(pack->body());
    } else if (auto arr = def->isa<Arr>()) {
        return contains_mem(arr->body());
    }
}

const Def* equip_mem(const Def* def) {
    auto& world  = def->world();
    auto memType = mem::type_mem(world);
    if (contains_mem(def)) { return def; }

    if (def->isa<Sigma>()) {
        size_t size = def->num_ops() + 1;
        DefArray newOps(size, [&](size_t i) { return i == 0 ? memType : def->op(i - 1); });

        return world.sigma(newOps);
    } else if (auto pack = def->isa<Pack>()) {
        auto count = as_lit(pack->shape());
        DefArray newOps(count + 1, [&](size_t i) { return i == 0 ? memType : pack->body(); });

        return world.sigma(newOps);
    } else if (auto arr = def->isa<Arr>()) {
        auto count = as_lit(arr->shape());
        DefArray newOps(count + 1, [&](size_t i) { return i == 0 ? memType : arr->body(); });

        return world.sigma(newOps);
    } else {
        return world.sigma({memType, def});
    }
}

const Def* continuation_codom(const Def* E) {
    auto pi = E->as<Pi>();
    assert(pi != NULL);
    return pi->dom(1)->as<Pi>()->dom();
}

bool is_continuation_type(const Def* E) {
    if (auto pi = E->isa<Pi>()) { return pi->codom()->isa<Bot>(); }
    return false;
}

bool is_continuation(const Def* e) { return is_continuation_type(e->type()); }

bool is_returning_continuation(const Def* e) {
    auto E = e->type();
    if (auto pi = E->isa<Pi>()) {
        // R world.DLOG("codom is {}", pi->codom());
        // R world.DLOG("codom kind is {}", pi->codom()->node_name());
        //  duck-typing applies here
        //  use short-circuit evaluation to reuse previous results
        return is_continuation_type(pi) &&       // continuation
               pi->num_doms() == 2 &&            // args, return
               is_continuation_type(pi->dom(1)); // return type
    }
    return false;
}

bool is_open_continuation(const Def* e) { return is_continuation(e) && !is_returning_continuation(e); }

bool is_direct_style_function(const Def* e) {
    // codom != Bot
    return e->type()->isa<Pi>() && !is_continuation(e);
}

const Def* continuation_dom(const Def* E) {
    auto pi = E->as<Pi>();
    assert(pi != NULL);
    if (pi->num_doms() == 0) { return pi->dom(); }
    return pi->dom(0);
}

/// high level
/// f: B -> C
/// g: A -> B
/// f o g := λ x. f(g(x)) : A -> C
/// with cps style
/// f: cn[B, cn C]
/// g: cn[A, cn B]
/// h = f o g
/// h : cn[A, cn C]
/// h = λ a ret_h. g(a,h')
/// h' : cn B
/// h' = λ b. f(b,ret_h)
const Def* compose_continuation(const Def* f, const Def* g) {
    assert(is_continuation(f));
    auto& world = f->world();
    world.DLOG("compose f (B->C): {} : {}", f, f->type());
    world.DLOG("compose g (A->B): {} : {}", g, g->type());
    assert(is_returning_continuation(f));
    assert(is_returning_continuation(g));

    f = lam_mem_wrap(f);
    g = lam_mem_wrap(g);

    auto F = f->type()->as<Pi>();
    auto G = g->type()->as<Pi>();

    auto is_mem = match<mem::M>(F->dom(0)->proj(0));

    F->dump();
    G->dump();

    auto A = continuation_dom(G);
    auto B = continuation_codom(G);

    auto B_hat = continuation_dom(F);
    auto C     = continuation_codom(F);
    // better handled by application type checks
    // auto B2 = continuation_dom(F);
    // assert(B == B2);

    world.DLOG("compose f (B->C): {} : {}", f, F);
    world.DLOG("compose g (A->B): {} : {}", g, G);
    world.DLOG("  A: {}", A);
    world.DLOG("  B: {}", B);
    world.DLOG("  C: {}", C);

    auto H     = world.cn({A, world.cn(C)});
    auto Hcont = world.cn(B);

    auto h     = world.nom_lam(H, world.dbg("comp_" + f->name() + "_" + g->name()));
    auto hcont = world.nom_lam(Hcont, world.dbg("comp_" + f->name() + "_" + g->name() + "_cont"));

    h->app(true, g, {h->var((nat_t)0), hcont});

    hcont->app(true, f,
               {
                   hcont->var(), // Warning: not var(0) => only one var => normalization flattens tuples down here
                   h->var(1)     // ret_var
               });

    return h;
}

} // namespace thorin

void findAndReplaceAll(std::string& data, std::string toSearch, std::string replaceStr) {
    size_t pos = data.find(toSearch);
    while (pos != std::string::npos) {
        data.replace(pos, toSearch.size(), replaceStr);
        pos = data.find(toSearch, pos + replaceStr.size());
    }
}
