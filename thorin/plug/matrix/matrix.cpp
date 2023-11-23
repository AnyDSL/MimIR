
#include "thorin/plug/matrix/matrix.h"

#include <thorin/pass/pass.h>
#include <thorin/plugin.h>

#include "thorin/plug/compile/pass/internal_cleanup.h"
#include "thorin/plug/matrix/pass/lower_matrix_highlevel.h"
#include "thorin/plug/matrix/pass/lower_matrix_lowlevel.h"
#include "thorin/plug/matrix/pass/lower_matrix_mediumlevel.h"
#include "thorin/plug/refly/refly.h"

using namespace thorin;
using namespace thorin::plug;

extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {
        "matrix", [](Normalizers& normalizers) { matrix::register_normalizers(normalizers); },
        [](Passes& passes) {
            register_pass<matrix::lower_matrix_high_level_map_reduce, thorin::plug::matrix::LowerMatrixHighLevelMapRed>(
                passes);
            register_pass<matrix::lower_matrix_medium_level, thorin::plug::matrix::LowerMatrixMediumLevel>(passes);
            register_phase<matrix::lower_matrix_low_level, thorin::plug::matrix::LowerMatrixLowLevel>(passes);
            register_pass<matrix::internal_map_reduce_cleanup, thorin::plug::compile::InternalCleanup>(passes,
                                                                                                       INTERNAL_PREFIX);
        },
        nullptr};
}
