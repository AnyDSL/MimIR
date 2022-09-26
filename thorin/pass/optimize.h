#pragma once

namespace thorin {

static constexpr int Pass_Internal_Priority = 50;
static constexpr int Pass_Default_Priority  = 100;
static constexpr int Opt_Phase              = 100;
static constexpr int Codegen_Prep_PHASE     = 300;

class World;
class PipelineBuilder;

void optimize(World&, PipelineBuilder&);

} // namespace thorin
