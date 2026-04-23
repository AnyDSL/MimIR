#pragma once

#include "mim/schedule.h"

namespace mim::plug::spirv {

/// Schedules mutables for SPIR-V emission.
/// Computes a sflow::CFG from the nest root and uses its loop hierarchy to
/// ensure that all blocks belonging to a loop are emitted contiguously after
/// the loop header, as required by SPIR-V's structured control flow rules.
Scheduler::Schedule schedule(const Nest& nest);

} // namespace mim::plug::spirv
