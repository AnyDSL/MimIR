#include "thorin/plug/db/db.h"

#include <thorin/plugin.h>

#include <thorin/pass/pass.h>

using namespace thorin;

extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"db", [](Normalizers& normalizers) { plug::db::register_normalizers(normalizers); }, nullptr, nullptr};
}
