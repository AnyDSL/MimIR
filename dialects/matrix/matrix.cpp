
#include "dialects/matrix/matrix.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "dialects/matrix/passes/lower_matrix_highlevel.h"
#include "dialects/matrix/passes/lower_matrix_lowlevel.h"
#include "dialects/matrix/passes/lower_matrix_mediumlevel.h"

using namespace thorin;

extern "C" THORIN_EXPORT DialectInfo thorin_get_dialect_info() {
    return {"matrix",
            [](PipelineBuilder& builder) {
                // Ordering in a phase is non-deterministic
                auto base = 150;
                builder.extend_opt_phase(
                    base + 0, [](thorin::PassMan& man) { man.add<thorin::matrix::LowerMatrixHighLevelMapRed>(); });
                builder.extend_opt_phase(
                    base + 1, [](thorin::PassMan& man) { man.add<thorin::matrix::LowerMatrixMediumLevel>(); });
                builder.append_phase(
                    base + 2, [](thorin::Pipeline& pipeline) { pipeline.add<thorin::matrix::LowerMatrixLowLevel>(); });
                // builder.extend_opt_phase(base + 2,
                //                          [](thorin::PassMan& man) { man.add<thorin::matrix::LowerMatrixLowLevel>();
                //                          });
            },
            nullptr, [](Normalizers& normalizers) { matrix::register_normalizers(normalizers); }};
}
