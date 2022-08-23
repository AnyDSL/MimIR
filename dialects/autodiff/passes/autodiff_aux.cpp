#include "autodiff_aux.h"

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
    auto& world = A->world();
    auto pb_ty  = pullback_type(E, A);
    auto pb     = world.nom_lam(pb_ty, world.dbg("zero_pb"));
    world.DLOG("zero_pullback {} -> {}", E, A);
    pb->app(true, pb->var(1), op_zero(A));
    return pb;
}

bool is_closed_function(const Def* e) {
    auto& world = e->world();
    auto E      = e->type();
    if (auto pi = E->isa<Pi>()) {
        // R world.DLOG("codom is {}", pi->codom());
        // R world.DLOG("codom kind is {}", pi->codom()->node_name());
        //  duck-typing applies here
        //  use short-circuit evaluation to reuse previous results
        return pi->codom()->isa<Bot>() != NULL && // continuation
               pi->num_doms() == 2 &&             // args, return
               pi->dom(1)->isa<Pi>() != NULL;     // return type
    }
    return false;
}

} // namespace thorin::autodiff