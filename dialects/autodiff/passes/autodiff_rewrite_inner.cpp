#include "dialects/autodiff/autodiff.h"
#include "dialects/autodiff/passes/autodiff_aux.h"
#include "dialects/autodiff/passes/autodiff_eval.h"

namespace thorin::autodiff {

/// rewrite the given definition
///
const Def* AutoDiffEval::augment_(const Def* def, Lam* f, Lam* f_diff) {
    auto& world           = def->world();
    auto f_arg_tangent_ty = tangent_type(f->type()->dom(0));
    // auto f_arg_tangent_ty =
    //     f_diff->type()->dom(1)->as<Pi>() // return type
    //     ->dom(1)->as<Pi>() // pb type
    //     ->dom(1)->as<Pi>()->dom(); // arg tangent type

    world.DLOG("Augment def {} : {}", def, def->type());

    // app => cont, operator, function
    if (auto app = def->isa<App>()) {
        auto callee = app->callee();
        auto arg    = app->arg();
        world.DLOG("Augment application: app {} with {}", callee, arg);

        auto aug_arg    = augment(arg, f, f_diff);
        auto aug_callee = augment(callee, f, f_diff);
        auto arg_ppb    = partial_pullback[aug_arg];

        if (!is_closed_function(callee)) {
            // TODO: check if function (not operator)
            // original function = unclosed function (return cont / continuation)
            //   Cn[E]
            // aug_calle (looks like a function but is not really)
            //   Cn[E, Cn[E, Cn[A]]]

            // ret(e) => ret'(e, e*)

            world.DLOG("continuation {} : {} => {} : {}", callee, callee->type(), aug_callee, aug_callee->type());

            auto arg_pb  = partial_pullback[aug_arg];
            auto aug_app = world.app(aug_callee, {aug_arg, arg_pb});
            world.DLOG("Augmented application: {} : {}", aug_app, aug_app->type());
            // assert(false);
            return aug_app;
        }

        // return def;
    }

    // projection
    else if (auto ext = def->isa<Extract>()) {
        // world.DLOG("Augment extract: {}", def);
        auto tuple = ext->tuple();
        auto index = ext->index();
        world.DLOG("Augment extract: {} #[{}]", tuple, index);

        auto aug_tuple = augment(tuple, f, f_diff);
        auto aug_index = augment(index, f, f_diff);

        auto shadow_tuple_pb = shadow_pullback[aug_tuple];

        auto aug_ext              = world.extract(aug_tuple, aug_index);
        auto pb                   = world.extract(shadow_tuple_pb, aug_index);
        partial_pullback[aug_ext] = pb;

        return aug_ext;
    }

    // vars (function argument)
    else if (auto var = def->isa<Var>()) {
        world.DLOG("Augment variable: {}", def);
        assert(augmented.count(def));
        auto aug_var = augmented[def];
        assert(partial_pullback.count(aug_var));
        return var;

        // auto var_ppb = partial_pullbacks[var];
        // if(var_ppb) {
        //     world.DLOG("Augment var: {} with {}", var, var_ppb);
        //     return var_ppb;
        // }
        // return def;

        // TODO: move out to general handling?
    }

    // constants
    else if (auto lit = def->isa<Lit>()) {
        world.DLOG("Augment literal: {}", def);
        auto aug_lit = lit;
        // set zero pullback
        auto pb                   = zero_pullback(lit->type(), f_arg_tangent_ty);
        partial_pullback[aug_lit] = pb;
        return def;
    }

    // TODO: tuple

    world.ELOG("did not expect to augment: {}", def);
    world.ELOG("node: {}", def->node_name());
    assert(false && "augment only on app");
}

} // namespace thorin::autodiff