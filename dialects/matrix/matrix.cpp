
#include "dialects/matrix/matrix.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "dialects/compile/passes/internal_cleanup.h"
#include "dialects/matrix/passes/lower_matrix_highlevel.h"
#include "dialects/matrix/passes/lower_matrix_lowlevel.h"
#include "dialects/matrix/passes/lower_matrix_mediumlevel.h"
#include "dialects/refly/refly.h"

using namespace thorin;

extern "C" THORIN_EXPORT DialectInfo thorin_get_dialect_info() {
    return {"matrix",
            [](Passes& passes) {
                register_pass<matrix::lower_matrix_high_level_map_reduce, thorin::matrix::LowerMatrixHighLevelMapRed>(
                    passes);
                register_pass<matrix::lower_matrix_medium_level, thorin::matrix::LowerMatrixMediumLevel>(passes);
                register_phase<matrix::lower_matrix_low_level, thorin::matrix::LowerMatrixLowLevel>(passes);
                register_pass<matrix::internal_map_reduce_cleanup, thorin::compile::InternalCleanup>(passes,
                                                                                                     INTERNAL_PREFIX);

                //     base + 0, [](thorin::PassMan& man) { man.add<thorin::matrix::LowerMatrixHighLevelMapRed>(); });
                // builder.extend_opt_phase(
                //     base + 1, [](thorin::PassMan& man) { man.add<thorin::matrix::LowerMatrixMediumLevel>(); });
                // builder.append_phase(
                //     base + 2, [](thorin::Pipeline& pipeline) { pipeline.add<thorin::matrix::LowerMatrixLowLevel>();
                //     });
                // builder.append_phase(
                //     base + 3, [](thorin::Pipeline& pipeline) { pipeline.add<thorin::refly::InternalCleanup>(); });
            },
            nullptr, [](Normalizers& normalizers) { matrix::register_normalizers(normalizers); }};
}
