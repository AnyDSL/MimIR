#include "mim/plug/matrix/matrix.h"

#include <mim/plugin.h>

#include <mim/pass/pass.h>

#include <mim/plug/compile/pass/internal_cleanup.h>

#include "mim/plug/matrix/pass/lower_matrix_highlevel.h"
#include "mim/plug/matrix/pass/lower_matrix_lowlevel.h"
#include "mim/plug/matrix/pass/lower_matrix_mediumlevel.h"

using namespace mim;
using namespace mim::plug;

void reg_stages(Phases& phases, Passes& passes) {
    Pipeline::hook<matrix::lower_matrix_low_level, mim::plug::matrix::LowerMatrixLowLevel>(phases);

    PassMan::hook<matrix::lower_matrix_high_level_map_reduce, mim::plug::matrix::LowerMatrixHighLevelMapRed>(passes);
    PassMan::hook<matrix::lower_matrix_medium_level, mim::plug::matrix::LowerMatrixMediumLevel>(passes);
    PassMan::hook<matrix::internal_map_reduce_cleanup, mim::plug::compile::InternalCleanup>(passes, INTERNAL_PREFIX);
}

extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"matrix", [](Normalizers& n) { matrix::register_normalizers(n); }, reg_stages, nullptr};
}
