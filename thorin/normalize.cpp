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

/*
 * instantiate templates
 */

#define CODE(T, o) template const Def* normalize_##T<T::o>(const Def*, const Def*, const Def*, const Def*);
THORIN_PE(CODE)
#undef CODE

} // namespace thorin
