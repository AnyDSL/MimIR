#pragma once

#include <cstdint>

#include <unordered_map>
#include <vector>

#include "dialects/regex/pass/automaton.h"

namespace thorin::automaton {
class DFANode {
public:
    DFANode() = default;

    void add_transition(const DFANode* to, std::uint16_t c);
    const DFANode* get_transition(std::uint16_t c) const;

    // F: void(const DFANode*)
    template<class F> void for_transitions(F&& f, std::uint16_t c) const {
        if (auto it = transitions_.find(c); it != transitions_.end()) f(it->second);
    }

    // F: void(std::uint16_t, const DFANode*)
    template<class F> void for_transitions(F&& f) const {
        for (auto& [c, to] : transitions_) f(c, to);
    }

    bool is_accepting() const { return accepting_; }
    void set_accepting(bool accepting) { accepting_ = accepting; }

    void dump() const;

private:
    std::unordered_map<std::uint16_t, const DFANode*> transitions_;
    bool accepting_ = false;
};

extern template class AutomatonBase<DFANode>;

class DFA : public AutomatonBase<DFANode> {
public:
    DFA()                      = default;
    DFA(const DFA&)            = delete;
    DFA& operator=(const DFA&) = delete;

    enum SpecialTransitons : std::uint16_t {
        ANY = 0x8001,
    };
};

} // namespace thorin::automaton
