#pragma once

#include <functional>
#include <vector>

#include "thorin/pass/pass.h"

namespace thorin {

class PipelineBuilder {
public:
    explicit PipelineBuilder() {}

    void extend_opt_phase(std::function<void(PassMan&)>);
    void extend_codegen_prep_phase(std::function<void(PassMan&)>);

    PassMan opt_phase(World& world);
    PassMan codegen_prep_phase(World& world);

private:
    std::vector<std::function<void(PassMan&)>> opt_phase_extensions_;
    std::vector<std::function<void(PassMan&)>> codegen_prep_phase_extensions_;
};

} // namespace thorin
