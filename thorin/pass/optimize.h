#pragma once

namespace thorin {

const int PASS_INTERNAL_PRIORITY = 50;
const int PASS_DEFAULT_PRIORITY  = 100;
const int OPT_PHASE              = 100;
const int CODEGEN_PREP_PHASE     = 300;

class World;
class PipelineBuilder;

void optimize(World&, PipelineBuilder&);

} // namespace thorin
