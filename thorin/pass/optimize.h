#pragma once

#include "thorin/util/types.h"

#include "absl/container/flat_hash_map.h"

namespace thorin {

static constexpr int Pass_Internal_Priority = 50;
static constexpr int Pass_Default_Priority  = 100;
static constexpr int Opt_Phase              = 100;
static constexpr int Codegen_Prep_Phase     = 300;

class World;
class PipelineBuilder;
using Passes = absl::flat_hash_map<flags_t, std::function<void(PipelineBuilder&)>>;

void optimize(World&, Passes&, PipelineBuilder&);

} // namespace thorin
