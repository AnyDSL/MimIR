#include "thorin/normalize.h"

#include "thorin/def.h"
#include "thorin/world.h"

namespace thorin {

template<PE op>
const Def* normalize_PE(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto& world = type->world();

    if constexpr (op == PE::known) {
        if (isa<Tag::PE>(PE::hlt, arg)) return world.lit_ff();
        if (arg->dep_const()) return world.lit_tt();
    }

    return world.raw_app(callee, arg, dbg);
}

const Def* normalize_zip(const Def* type, const Def* c, const Def* arg, const Def* dbg) {
    auto& w                    = type->world();
    auto callee                = c->as<App>();
    auto is_os                 = callee->arg();
    auto [n_i, Is, n_o, Os, f] = is_os->projs<5>();
    auto [r, s]                = callee->decurry()->args<2>();
    auto lr                    = isa_lit(r);
    auto ls                    = isa_lit(s);

    // TODO commute
    // TODO reassociate
    // TODO more than one Os
    // TODO select which Is/Os to zip

    if (lr && ls && *lr == 1 && *ls == 1) return w.app(f, arg, dbg);

    if (auto l_in = isa_lit(n_i)) {
        auto args = arg->projs(*l_in);

        if (lr && std::ranges::all_of(args, [](auto arg) { return is_tuple_or_pack(arg); })) {
            auto shapes = s->projs(*lr);
            auto s_n    = isa_lit(shapes.front());

            if (s_n) {
                DefArray elems(*s_n, [&, f = f](size_t s_i) {
                    DefArray inner_args(args.size(), [&](size_t i) { return args[i]->proj(*s_n, s_i); });
                    if (*lr == 1)
                        return w.app(f, inner_args);
                    else
                        return w.app(
                            w.app(w.app(w.ax_zip(), {w.lit_nat(*lr - 1), w.tuple(shapes.skip_front())}), is_os),
                            inner_args);
                });
                return w.tuple(elems);
            }
        }
    }

    return w.raw_app(callee, arg, dbg);
}

/*
 * instantiate templates
 */

#define CODE(T, o) template const Def* normalize_ ## T<T::o>(const Def*, const Def*, const Def*, const Def*);
THORIN_PE(CODE)
#undef CODE

} // namespace thorin
