#pragma once

#include <cstdint>

#include <iostream>
#include <list>
#include <set>
#include <unordered_map>
#include <vector>

namespace thorin {
namespace automaton {

class DFANode;
class NFANode;

template<class NodeType> class AutomatonBase {
public:
    AutomatonBase()                                = default;
    AutomatonBase(const AutomatonBase&)            = delete;
    AutomatonBase& operator=(const AutomatonBase&) = delete;

    NodeType* add_state() {
        nodes_.emplace_back();
        return &nodes_.back();
    }

    void set_start(const NodeType* start) { start_ = start; }

    const NodeType* get_start() const { return start_; }

    std::set<const NodeType*> get_reachable_states() const {
        std::set<const NodeType*> reachableStates;
        std::vector<const NodeType*> workList;
        workList.push_back(get_start());
        while (!workList.empty()) {
            auto state = workList.back();
            workList.pop_back();
            reachableStates.insert(state);
            state->for_transitions([&](auto, auto to) {
                if (!reachableStates.contains(to)) workList.push_back(to);
            });
        }
        return reachableStates;
    }

    friend std::ostream& operator<<(std::ostream& os, const AutomatonBase& automaton) {
        if constexpr(std::is_same_v<NodeType, DFANode>)
            os << "digraph dfa {\n";
        else if constexpr(std::is_same_v<NodeType, NFANode>)
            os << "digraph nfa {\n";
        else
            os << "digraph automaton {\n";
        os << "  start -> \"" << automaton.start_ << "\";\n";

        for (auto& node : automaton.nodes_) os << node;
        os << "}\n";
        return os;
    }

private:
    std::list<NodeType> nodes_;
    const NodeType* start_ = nullptr;
};

} // namespace automaton
} // namespace thorin
