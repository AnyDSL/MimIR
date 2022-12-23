#include "dialects/affine/affine.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "dialects/affine/passes/lower_for.h"

using namespace thorin;

extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info() {
    return {"affine", [](Passes& passes) { register_pass<affine::lower_for_pass, affine::LowerFor>(passes); }, nullptr,
            nullptr};
}
