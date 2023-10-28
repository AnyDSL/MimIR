#pragma once

#include <cstdint>

#include <iostream>
#include <list>
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

    void dump() const {
        std::cout << "digraph beep {\n";
        std::cout << "  start -> \"" << start_ << "\";\n";

        for (auto& node : nodes_) {
            node.dump();}
        std::cout << "}\n";
    }

private:
    std::list<NodeType> nodes_;
    const NodeType* start_ = nullptr;
};

} // namespace automaton
} // namespace thorin