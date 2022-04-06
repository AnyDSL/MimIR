#include "thorin/pass/fp/split_slots.h"

#include "thorin/pass/fp/beta_red.h"
#include "thorin/pass/fp/eta_red.h"

namespace thorin {

const Def* SplitSlots::rewrite(const Def* def) {
    if (auto slot = isa<Tag::Slot>(def)) {
        auto [mem, id]             = slot->args<2>();
        auto [_, ptr]              = slot->projs<2>();
        auto [pointee, addr_space] = as<Tag::Ptr>(ptr->type())->args<2>();

        if (auto sigma = pointee->isa<Sigma>()) {
            for (size_t i = 0, e = sigma->num_ops(); i != e; ++i) {}
        }
    }

    return def;
}

undo_t SplitSlots::analyze(const Def* def) { return No_Undo; }

} // namespace thorin
