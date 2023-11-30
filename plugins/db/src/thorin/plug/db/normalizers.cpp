#include "thorin/world.h"

#include "thorin/plug/db/db.h"

namespace thorin::plug::db {

Ref normalize_step(Ref type, Ref callee, Ref arg) {
    auto& world = type->world();
    std::cout << "hello" << std::endl;
    return world.raw_app(type, callee, arg);
}

THORIN_db_NORMALIZER_IMPL

} // namespace thorin::plug::db
