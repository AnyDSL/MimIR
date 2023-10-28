#include "dfa.h"

#include <iostream>

#include "dialects/regex/pass/automaton.h"

using namespace thorin::automaton;

void DFANode::add_transition(const DFANode* to, std::uint16_t c) {
    if (auto it = transitions_.find(c); it != transitions_.end())
        it->second = to;
    else
        transitions_.emplace(c, to);
}

const DFANode* DFANode::get_transition(std::uint16_t c) const {
    if (auto it = transitions_.find(c); it != transitions_.end())
        return it->second;
    else
        return {};
}

void DFANode::dump() const {
    auto print_char = [](std::uint16_t c) -> std::string {
        if (c == DFA::SpecialTransitons::ANY)
            return ".";
        else
            return {1, static_cast<char>(c)};
    };

    if (this->is_accepting()) std::cout << "  \"" << this << "\" [shape=doublecircle];\n";

    for (auto& [c, to] : transitions_)
        std::cout << "  \"" << this << "\" -> \"" << to << "\" [label=\"" << print_char(c) << "\"];\n";
}

namespace thorin::automaton {
template class AutomatonBase<DFANode>;
}
