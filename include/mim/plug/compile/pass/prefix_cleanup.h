#pragma once

#include <mim/def.h>

#include <mim/pass/pass.h>

namespace mim::plug::compile {

class PrefixCleanup : public RWPass<PrefixCleanup, Lam> {
public:
    PrefixCleanup(PassMan& man, std::string prefix = "internal_")
        : RWPass(man, "prefix_cleanup " + prefix)
        , prefix_(std::move(prefix)) {}

    void enter() override;

private:
    std::string prefix_;
};

} // namespace mim::plug::compile
