#include "mim/plug/direct/direct.h"

#include <mim/pass/pipelinebuilder.h>
#include <mim/plugin.h>

#include "mim/plug/direct/pass/cps2ds.h"
#include "mim/plug/direct/pass/ds2cps.h"

using namespace mim;
using namespace mim::plug;

extern "C" MIM_EXPORT Plugin mim_get_plugin() {
    return {"direct", [](Normalizers& normalizers) { direct::register_normalizers(normalizers); },
            [](Passes& passes) {
                register_pass<direct::ds2cps_pass, direct::DS2CPS>(passes);
                register_pass<direct::cps2ds_pass, direct::CPS2DS>(passes);
            },
            nullptr};
}
