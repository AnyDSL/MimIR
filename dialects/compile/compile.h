#pragma once

#include <thorin/world.h>

#include "thorin/pass/pipelinebuilder.h"

#include "dialects/compile/autogen.h"

namespace thorin::compile {
void handle_optimization_part(const Def* part, World& world, Passes& passes, PipelineBuilder& builder);
}
