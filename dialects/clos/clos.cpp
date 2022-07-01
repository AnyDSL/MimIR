#include "dialects/clos/clos.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

using namespace thorin;

extern "C" THORIN_EXPORT DialectInfo thorin_get_dialect_info() {
    return {"clos", [](PipelineBuilder& builder) {}, nullptr,
            [](Normalizers& normalizers) { clos::register_normalizers(normalizers); }};
}
