#include "mim/plug/matrix/matrix.h"

#include <mim/plugin.h>

#include <mim/pass/pass.h>

#include "mim/plug/matrix/pass/lower_matrix_highlevel.h"
#include "mim/plug/matrix/pass/lower_matrix_lowlevel.h"
#include "mim/plug/matrix/pass/lower_matrix_mediumlevel.h"

using namespace mim;
using namespace mim::plug;

void reg_stages(Phases& phases, Passes& passes) {
    Pipeline::hook<matrix::lower_matrix_low_level, matrix::LowerMatrixLowLevel>(phases);
    // clang-format off
    PassMan::hook<matrix::lower_matrix_high_level_map_reduce, matrix::LowerMatrixHighLevelMapRed>(passes);
    PassMan::hook<matrix::lower_matrix_medium_level,          matrix::LowerMatrixMediumLevel    >(passes);
    // clang-format on
}

extern "C" MIM_EXPORT Plugin mim_get_plugin() { return {"matrix", matrix::register_normalizers, reg_stages, nullptr}; }
