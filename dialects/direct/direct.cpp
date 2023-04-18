#include "dialects/direct/direct.h"

#include <thorin/pass/pipelinebuilder.h>
#include <thorin/plugin.h>

#include "dialects/direct/passes/cps2ds.h"
#include "dialects/direct/passes/ds2cps.h"

using namespace thorin;

extern "C" THORIN_EXPORT Plugin thorin_get_plugin() {
    return {"direct", [](Normalizers& normalizers) { direct::register_normalizers(normalizers); },
            [](Passes& passes) {
                register_pass<direct::ds2cps_pass, direct::DS2CPS>(passes);
                register_pass<direct::cps2ds_pass, direct::CPS2DS>(passes);
            },
            nullptr};
}
