#include "thorin/world.h"

#include "dialects/fun/fun.h"

namespace thorin::fun {

Ref normalize_zip(Ref type, Ref c, Ref arg) {
    auto& w                     = type->world();
    auto callee                 = c->as<App>();
    auto [r, s, ni, Is, no, Os] = callee->decurry()->args<6>();
    auto f                      = callee->arg();
    auto lr                     = Lit::isa(r);
    auto ls                     = Lit::isa(s);

    // TODO commute
    // TODO reassociate
    // TODO more than one Os
    // TODO select which Is/Os to zip

    if (lr && ls && *lr == 1 && *ls == 1) return w.app(f, arg);

    if (auto lni = Lit::isa(ni)) {
        auto args = arg->projs(*lni);

        if (lr && std::ranges::all_of(args, [](Ref arg) { return arg->isa<Tuple, Pack>(); })) {
            auto shapes = s->projs(*lr);
            auto s_n    = Lit::isa(shapes.front());

            // recursively peel off and fold front dimension, if possible
            if (s_n) {
                DefArray elems(*s_n, [&, f = f](size_t s_i) {
                    DefArray peel(args.size(), [&](size_t i) { return args[i]->proj(*s_n, s_i); });
                    if (*lr == 1) {
                        return w.app(f, peel);
                    } else {
                        auto shape   = w.tuple(shapes.skip_front());
                        auto app_zip = w.app(w.annex<zip>(), {w.lit_nat(*lr - 1), shape, ni, Is, no, Os});
                        return w.app(w.app(app_zip, f), peel);
                    }
                });
                return w.tuple(elems);
            }
        }
    }

    return w.raw_app(type, callee, arg);
}

Ref normalize_map(Ref type, Ref c, Ref arg) {
    auto& w     = type->world();
    auto callee = c->as<App>();
    // TODO
    return w.raw_app(type, callee, arg);
}

Ref normalize_reduce(Ref type, Ref c, Ref arg) {
    auto& w     = type->world();
    auto callee = c->as<App>();
    // TODO
    return w.raw_app(type, callee, arg);
}

THORIN_fun_NORMALIZER_IMPL

} // namespace thorin::fun
