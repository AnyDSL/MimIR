#include <mim/config.h>
#include <mim/pass.h>

#include "mim/plug/aie2p/autogen.h"
#include "mim/plug/aie2p/phase/lower_aie2p.h"

namespace mim::plug::aie2p {
void register_stages(mim::Flags2Stages& stages) {
    Stage::hook<aie2p::lower_aie2p_phase, aie2p::phase::LowerAIE2P>(stages);
}
} // namespace mim::plug::aie2p

extern "C" MIM_EXPORT mim::Plugin mim_get_plugin() {
    return {
        "aie2p",
        nullptr, // normalizers
        mim::plug::aie2p::register_stages,
        nullptr // backends
    };
}
