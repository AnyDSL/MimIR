
#include "dialects/matrix/matrix.h"

#include <thorin/pass/pass.h>
#include <thorin/plugin.h>

#include "dialects/compile/pass/internal_cleanup.h"
#include "dialects/matrix/pass/lower_matrix_highlevel.h"
#include "dialects/matrix/pass/lower_matrix_lowlevel.h"
#include "dialects/matrix/pass/lower_matrix_mediumlevel.h"
#include "dialects/refly/refly.h"

using namespace thorin;

extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"matrix", [](Normalizers& normalizers) { matrix::register_normalizers(normalizers); },
            [](Passes& passes) {
                register_pass<matrix::lower_matrix_high_level_map_reduce, thorin::matrix::LowerMatrixHighLevelMapRed>(
                    passes);
                register_pass<matrix::lower_matrix_medium_level, thorin::matrix::LowerMatrixMediumLevel>(passes);
                register_phase<matrix::lower_matrix_low_level, thorin::matrix::LowerMatrixLowLevel>(passes);
                register_pass<matrix::internal_map_reduce_cleanup, thorin::compile::InternalCleanup>(passes,
                                                                                                     INTERNAL_PREFIX);
            },
            nullptr};
}
