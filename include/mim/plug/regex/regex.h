#pragma once

#include "mim/plug/regex/autogen.h"

namespace mim::plug::regex::detail {
template<class ConjOrDisj>
void flatten_in_arg(const Def* arg, DefVec& new_args) {
    for (const auto* proj : arg->projs()) {
        // flatten conjs in conjs / disj in disjs
        if (auto seq_app = Axm::isa<ConjOrDisj>(proj))
            flatten_in_arg<ConjOrDisj>(seq_app->arg(), new_args);
        else
            new_args.push_back(proj);
    }
}

template<class ConjOrDisj>
DefVec flatten_in_arg(const Def* arg) {
    DefVec new_args;
    flatten_in_arg<ConjOrDisj>(arg, new_args);
    return new_args;
}
} // namespace mim::plug::regex
