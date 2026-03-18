#pragma once

#include <absl/container/flat_hash_map.h>

#include <mim/phase.h>

namespace mim::plug::aie2p::phase {

/// Lowers AIE2P intrinsic axioms to CPS-wrapped LLVM intrinsic calls.
/// Uses a map-based design: adding a new intrinsic = 1 line in the map + 1 line in .mim.
class LowerAIE2P : public RWPhase {
public:
    LowerAIE2P(World& world, flags_t annex)
        : RWPhase(world, annex) {}

    const Def* rewrite_imm_App(const App*) final;

private:
    const Def* lower_to_cps_intrinsic(const Def* arg_rewritten, const Def* dom, const Def* ret, const char* llvm_name);

    using IntrinsicMap = absl::flat_hash_map<flags_t, const char*>;
    static const IntrinsicMap intrinsic_names_;

    absl::flat_hash_map<flags_t, const Def*> wrapped_cache_;
};

} // namespace mim::plug::aie2p::phase
