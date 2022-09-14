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
    void extend_opt_prep_phase1(std::function<void(PassMan&)>);
    void extend_opt_prep_phase2(std::function<void(PassMan&)>);

    std::unique_ptr<PassMan> opt_phase(World& world);
    std::unique_ptr<PassMan> opt_phase2(World& world);
    std::unique_ptr<PassMan> codegen_prep_phase(World& world);
    std::unique_ptr<PassMan> opt_prep_phase1(World& world);
    std::unique_ptr<PassMan> opt_prep_phase2(World& world);
    void add_opt(PassMan man);

private:
    std::vector<std::function<void(PassMan&)>> opt_phase_extensions_;
    std::vector<std::function<void(PassMan&)>> codegen_prep_phase_extensions_;
    std::vector<std::function<void(PassMan&)>> opt_prep_phase1_extensions_;
    std::vector<std::function<void(PassMan&)>> opt_prep_phase2_extensions_;
};

} // namespace thorin
