#pragma once

#include <cstdint>

#include <iostream>
#include <list>
#include <set>
#include <unordered_map>
#include <vector>

#include "thorin/util/types.h"

#include "absl/container/flat_hash_map.h"
#include "plug/regex/range_helper.h"

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
        Vector<const NodeType*> workList;
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
        if constexpr (std::is_same_v<NodeType, DFANode>)
            os << "digraph dfa {\n";
        else if constexpr (std::is_same_v<NodeType, NFANode>)
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

template<class NodeType, class PrintCharF>
std::ostream& print_node(std::ostream& os, const NodeType& node, PrintCharF&& print_char) {
    if (node.is_accepting()) os << "  \"" << &node << "\" [shape=doublecircle];\n";
    if (node.is_erroring()) os << "  \"" << &node << "\" [shape=square];\n";

    absl::flat_hash_map<const NodeType*, Vector<std::pair<nat_t, nat_t>>> node2transitions;
    node.for_transitions([&](auto c, auto to) {
        if (!node2transitions.contains(to))
            node2transitions.emplace(to, Vector<std::pair<nat_t, nat_t>>{
                                             std::pair<nat_t, nat_t>{c, c}
            });
        else
            node2transitions[to].push_back({c, c});
    });

    for (auto& [to, ranges] : node2transitions) {
        std::sort(ranges.begin(), ranges.end(), regex::RangeCompare{});
        ranges = regex::merge_ranges(ranges);
        for (auto& [lo, hi] : ranges) {
            os << "  \"" << &node << "\" -> \"" << to << "\" [label=\"" << std::forward<PrintCharF>(print_char)(lo);
            if (lo != hi) os << "-" << std::forward<PrintCharF>(print_char)(hi);
            os << " (" << lo;
            if (lo != hi) os << "-" << hi;
            os << ")\"];\n";
        }
    }

    return os;
}

} // namespace automaton
} // namespace thorin
