#pragma once

#include <cstdint>

#include <vector>

#include <absl/container/flat_hash_map.h>

#include "thorin/plug/regex/automaton/automaton.h"

namespace thorin::automaton {
class NFANode {
public:
    NFANode() = default;

    void add_transition(const NFANode* to, std::uint16_t c);
    std::vector<const NFANode*> get_transitions(std::uint16_t c) const;

    // F: void(const NFANode*)
    template<class F> void for_transitions(F&& f, std::uint16_t c) const {
        if (erroring_) return;
        if (auto it = transitions_.find(c); it != transitions_.end())
            for (const auto& to : it->second) std::forward<F>(f)(to);
    }

    // F: void(std::uint16_t, const NFANode*)
    template<class F> void for_transitions(F&& f) const {
        if (erroring_) return;
        for (auto& [c, tos] : transitions_)
            for (const auto& to : tos) std::forward<F>(f)(c, to);
    }

    bool is_accepting() const { return accepting_; }
    void set_accepting(bool accepting) {
        assert(!(accepting && erroring_) && "state cannot be accepting and erroring");
        accepting_ = accepting;
    }

    bool is_erroring() const noexcept { return erroring_; }
    void set_erroring(bool erroring) noexcept {
        assert(!(accepting_ && erroring) && "state cannot be accepting and erroring");
        erroring_ = erroring;
    }

    friend std::ostream& operator<<(std::ostream& os, const NFANode& node);

private:
    absl::flat_hash_map<std::uint16_t, std::vector<const NFANode*>> transitions_;
    bool accepting_ = false;
    bool erroring_  = false;
};

extern template class AutomatonBase<NFANode>;

class NFA : public AutomatonBase<NFANode> {
public:
    NFA()                      = default;
    NFA(const NFA&)            = delete;
    NFA& operator=(const NFA&) = delete;

    enum SpecialTransitons : std::uint16_t {
        EPSILON = 0x8001,
    };
};

} // namespace thorin::automaton
