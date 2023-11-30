#include "thorin/world.h"

#include "thorin/plug/db/db.h"

namespace thorin::plug::db {

Ref normalize_const(Ref type, Ref, Ref arg) {
    auto& world = type->world();
    return world.lit(world.type_idx(arg), 42);
}

THORIN_db_NORMALIZER_IMPL

} // namespace thorin::plug::db
