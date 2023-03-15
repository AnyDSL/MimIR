#pragma once

#include "thorin/util/types.h"

#include "absl/container/flat_hash_map.h"

namespace thorin {

class World;
class PipelineBuilder;
class Def;
class Dialect;
using DefVec = std::vector<const Def*>;
// `axiom ↦ (pipeline part) × (axiom application) → ()`
// The function should inspect application to construct the pass/phase and add it to the pipeline.
using Passes = absl::flat_hash_map<flags_t, std::function<void(World&, PipelineBuilder&, const Def*)>>;

void optimize(World&);

} // namespace thorin
