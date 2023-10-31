#pragma once

#include <cstdint>

#include <iostream>
#include <list>
#include <set>
#include <unordered_map>
#include <vector>

namespace thorin {
namespace automaton {

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

    void dump(std::string_view name = "automaton") const {
        std::cout << "digraph " << name << " {\n";
        std::cout << "  start -> \"" << start_ << "\";\n";

        for (auto& node : nodes_) node.dump();
        std::cout << "}\n";
    }

private:
    std::list<NodeType> nodes_;
    const NodeType* start_ = nullptr;
};

} // namespace automaton
} // namespace thorin
