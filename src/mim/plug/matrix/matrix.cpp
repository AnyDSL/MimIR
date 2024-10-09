#include "mim/plug/matrix/matrix.h"

#include <mim/plugin.h>

#include <mim/pass/pass.h>

#include <mim/plug/compile/pass/internal_cleanup.h>

#include "mim/plug/matrix/pass/lower_matrix_highlevel.h"
#include "mim/plug/matrix/pass/lower_matrix_lowlevel.h"
#include "mim/plug/matrix/pass/lower_matrix_mediumlevel.h"

using namespace mim;
using namespace mim::plug;

extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {
        "matrix", [](Normalizers& normalizers) { matrix::register_normalizers(normalizers); },
        [](Passes& passes) {
            register_pass<matrix::lower_matrix_high_level_map_reduce, mim::plug::matrix::LowerMatrixHighLevelMapRed>(
                passes);
            register_pass<matrix::lower_matrix_medium_level, mim::plug::matrix::LowerMatrixMediumLevel>(passes);
            register_phase<matrix::lower_matrix_low_level, mim::plug::matrix::LowerMatrixLowLevel>(passes);
            register_pass<matrix::internal_map_reduce_cleanup, mim::plug::compile::InternalCleanup>(passes,
                                                                                                    INTERNAL_PREFIX);
        },
        nullptr};
}
