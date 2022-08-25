#include "dialects/autodiff/auxiliary/autodiff_aux.h"

#include "dialects/autodiff/autodiff.h"

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

bool is_continuation(const Def* e) {
    auto& world = e->world();
    auto E      = e->type();

    if (auto pi = E->isa<Pi>()) { return pi->codom()->isa<Bot>(); }
    return false;
}

bool is_closed_function(const Def* e) {
    auto& world = e->world();
    auto E      = e->type();
    if (auto pi = E->isa<Pi>()) {
        // R world.DLOG("codom is {}", pi->codom());
        // R world.DLOG("codom kind is {}", pi->codom()->node_name());
        //  duck-typing applies here
        //  use short-circuit evaluation to reuse previous results
        return is_continuation(pi) &&         // continuation
               pi->num_doms() == 2 &&         // args, return
               pi->dom(1)->isa<Pi>() != NULL; // return type
    }
    return false;
}

bool is_open_continuation(const Def* e) {
    auto& world = e->world();
    return is_continuation(e) && !is_closed_function(e);
}

bool is_direct_style_function(const Def* e) {
    auto& world = e->world();
    // codom != Bot
    return e->type()->isa<Pi>() && !is_continuation(e);
}

// P => P'
// TODO: function => extend
const Def* augment_type_fun(const Def* ty) { return ty; }
// P => P*
// TODO: nothing? function => R? Mem => R?
const Def* tangent_type_fun(const Def* ty) { return ty; }

/// computes pb type E* -> A*
/// E - type of the expression (return type for a function)
/// A - type of the argument (point of orientation resp. derivative - argument type for partial pullbacks)
const Pi* pullback_type(const Def* E, const Def* A) {
    auto& world   = E->world();
    auto tang_arg = tangent_type_fun(A);
    auto tang_ret = tangent_type_fun(E);
    auto pb_ty    = world.cn({tang_ret, world.cn({tang_arg})});
    return pb_ty;
}

// A,R => A'->R' * (R* -> A*)
const Pi* autodiff_type_fun(const Def* arg, const Def* ret) {
    auto& world  = arg->world();
    auto aug_arg = augment_type_fun(arg);
    auto aug_ret = augment_type_fun(ret);
    // Q* -> P*
    auto pb_ty = pullback_type(ret, arg);
    // P' -> Q' * (Q* -> P*)
    auto deriv_ty = world.cn({aug_arg, world.cn({aug_ret, pb_ty})});
    return deriv_ty;
}

// P->Q => P'->Q' * (Q* -> P*)
const Pi* autodiff_type_fun(const Def* ty) {
    auto& world = ty->world();
    // TODO: handle DS (operators)
    if (auto pi = ty->isa<Pi>()) {
        auto [arg, ret_pi] = pi->doms<2>();
        auto ret           = ret_pi->as<Pi>()->dom();
        return autodiff_type_fun(arg, ret);
    }
    // TODO: what is this object? (only numbers are printed)
    // possible abstract type from autodiff axiom
    world.WLOG("AutoDiff on type: {}", ty);
    // ty->dump(300);
    // world.write("tmp.txt");
    // can not work with
    // assert(false && "autodiff_type should not be invoked on non-pi types");
    return NULL;
}

const Def* zero_def(const Def* T) {
    auto& world = T->world();
    world.DLOG("zero_def for type {} <{}>", T, T->node_name());
    if (auto arr = T->isa<Arr>()) {
        auto shape      = arr->shape();
        auto body       = arr->body();
        auto inner_zero = zero_def(body);
        auto zero_arr   = world.pack(shape, inner_zero);
        world.DLOG("zero_def for array of shape {} with type {}", shape, body);
        world.DLOG("zero_arr: {}", zero_arr);
        return zero_arr;
        // }else if(auto lit = match<type_int_>()) { }
        // }else if(auto tint = T->isa<Tag::Int>()) {
    } else if (auto app = T->isa<App>()) {
        auto callee = app->callee();
        // auto args = app->args();
        world.DLOG("app callee: {} : {} <{}>", callee, callee->type(), callee->node_name());
        // TODO: can you directly match Tag::Int?
        if (callee == world.type_int()) {
            // auto size = app->arg(0);
            auto zero = world.lit_int(T, 0, world.dbg("zero"));
            // world.DLOG("zero_def for int of size {} is {}", size, zero);
            world.DLOG("zero_def for int is {}", zero);
            return zero;
        }
    }
    // assert(0);
    return nullptr;
}

} // namespace thorin::autodiff