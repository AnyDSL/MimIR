#include "thorin/normalize.h"

#include "thorin/def.h"
#include "thorin/world.h"

namespace thorin {

// TODO I guess we can do that with C++20 <bit>
inline u64 pad(u64 offset, u64 align) {
    auto mod = offset % align;
    if (mod != 0) offset += align - mod;
    return offset;
}

// TODO this currently hard-codes x86_64 ABI
// TODO in contrast to C, we might want to give singleton types like '.Idx 1' or '[]' a size of 0 and simply nuke each
// and every occurance of these types in a later phase
// TODO Pi and others
template<Trait op>
const Def* normalize_Trait(const Def*, const Def* callee, const Def* type, const Def* dbg) {
    auto& world = type->world();
#if 0

    // todo: figure out a way to normalize traits on dialect types..
    //if (auto ptr = isa<Tag::Ptr>(type)) {
        //return world.lit_nat(8);
    //} else
    if (type->isa<Pi>()) {
        return world.lit_nat(8); // Gets lowered to function ptr
    } else if (auto idx = type->isa<Idx>()) {
        if (idx->size()->isa<Top>()) return world.lit_nat(8);
        if (auto w = isa_lit(idx->size())) {
            if (*w == 0) return world.lit_nat(8);
            if (*w <= 0x0000'0000'0000'0100_u64) return world.lit_nat(1);
            if (*w <= 0x0000'0000'0001'0000_u64) return world.lit_nat(2);
            if (*w <= 0x0000'0001'0000'0000_u64) return world.lit_nat(4);
            return world.lit_nat(8);
        }
    }
    //else if (auto real = isa<Tag::Real>(type)) {
        //if (auto w = isa_lit(real->arg())) {
            //switch (*w) {
                //case 16: return world.lit_nat(2);
                //case 32: return world.lit_nat(4);
                //case 64: return world.lit_nat(8);
                //default: unreachable();
            //}
        //}
    //}
    else if (type->isa<Sigma>() || type->isa<Meet>()) {
        u64 offset = 0;
        u64 align  = 1;
        for (auto t : type->ops()) {
            auto a = isa_lit(world.op(Trait::align, t));
            auto s = isa_lit(world.op(Trait::size, t));
            if (!a || !s) goto out;

            align  = std::max(align, *a);
            offset = pad(offset, *a) + *s;
        }

        offset   = pad(offset, align);
        u64 size = std::max(1_u64, offset);

        switch (op) {
            case Trait::align: return world.lit_nat(align);
            case Trait::size: return world.lit_nat(size);
        }
    } else if (auto arr = type->isa_structural<Arr>()) {
        auto align = world.op(Trait::align, arr->body());
        if constexpr (op == Trait::align) return align;

        if (auto b = isa_lit(world.op(Trait::size, arr->body()))) {
            auto i64_t = world.type_int_(64);
            auto s     = world.op_bitcast(i64_t, arr->shape());
            auto mul   = world.op(Wrap::mul, WMode::nsw | WMode::nuw, world.lit_idx(i64_t, *b), s);
            return world.op_bitcast(world.type_nat(), mul);
        }
    } else if (auto join = type->isa<Join>()) {
        if (auto sigma = join->convert()) return world.op(op, sigma, dbg);
    }

out:
#endif
    return world.raw_app(callee, type, dbg);
}

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

// clang-format off
#define CODE(T, o) template const Def* normalize_ ## T<T::o>(const Def*, const Def*, const Def*, const Def*);
THORIN_TRAIT(CODE)
THORIN_PE   (CODE)
#undef CODE
// clang-format on

} // namespace thorin
