#pragma once

#include <cstdint>

#include <vector>

#include <absl/container/flat_hash_map.h>

#include "dialects/regex/pass/automaton.h"

namespace thorin::automaton {
class NFANode {
public:
    NFANode() = default;

    void add_transition(const NFANode* to, std::uint16_t c);
    std::vector<const NFANode*> get_transitions(std::uint16_t c) const;

    // F: void(const NFANode*)
    template<class F> void for_transitions(F&& f, std::uint16_t c) const {
        if (auto it = transitions_.find(c); it != transitions_.end())
            for (const auto& to : it->second) std::forward<F>(f)(to);
    }

    // F: void(std::uint16_t, const NFANode*)
    template<class F> void for_transitions(F&& f) const {
        for (auto& [c, tos] : transitions_)
            for (const auto& to : tos) std::forward<F>(f)(c, to);
    }

    bool is_accepting() const { return accepting_; }
    void set_accepting(bool accepting) { accepting_ = accepting; }

    friend std::ostream& operator<<(std::ostream& os, const NFANode& node);

private:
    absl::flat_hash_map<std::uint16_t, std::vector<const NFANode*>> transitions_;
    bool accepting_ = false;
};

extern template class AutomatonBase<NFANode>;

class NFA : public AutomatonBase<NFANode> {
public:
    NFA()                      = default;
    NFA(const NFA&)            = delete;
    NFA& operator=(const NFA&) = delete;

    enum SpecialTransitons : std::uint16_t {
        EPSILON = 0x8001,
        ANY     = 0x8002,
    };
};

} // namespace thorin::automaton
