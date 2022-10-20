
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
                builder.extend_opt_phase([](thorin::PassMan& man) {
                    man.add<thorin::matrix::LowerMatrixHighLevel>();
                    man.add<thorin::matrix::LowerMatrixMediumLevel>();
                    man.add<thorin::matrix::LowerMatrixLowLevel>();
                });
            },
            nullptr, [](Normalizers& normalizers) { matrix::register_normalizers(normalizers); }};
}
