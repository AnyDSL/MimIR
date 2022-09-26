#pragma once

#include <functional>
#include <vector>

#include "thorin/pass/optimize.h"
#include "thorin/pass/pass.h"

namespace thorin {

typedef std::function<void(PassMan&)> PassBuilder;
typedef std::pair<int, PassBuilder> PrioPassBuilder;
typedef std::vector<PrioPassBuilder> PassList;

struct passCmp {
    constexpr bool operator()(PrioPassBuilder const& a, PrioPassBuilder const& b) const noexcept {
        return a.first < b.first;
    }
};

class PipelineBuilder {
public:
    explicit PipelineBuilder() {}

    void extend_opt_phase(int i, std::function<void(PassMan&)>, int priority = Pass_Default_Priority);
    void extend_opt_phase(std::function<void(PassMan&)>&&);
    void add_opt(int i);
    void extend_codegen_prep_phase(std::function<void(PassMan&)>&&);

    std::unique_ptr<PassMan> opt_phase(int i, World& world);
    void add_opt(PassMan man);

    std::vector<int> passes();

private:
    std::map<int, PassList> phase_extensions_;
};

} // namespace thorin
