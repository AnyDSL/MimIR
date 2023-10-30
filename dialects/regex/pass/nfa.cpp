#include "nfa.h"

#include <iostream>

#include "dialects/regex/pass/automaton.h"

using namespace thorin::automaton;

void NFANode::add_transition(const NFANode* to, std::uint16_t c) {
    if (auto it = transitions_.find(c); it != transitions_.end())
        it->second.push_back(to);
    else
        transitions_.emplace(c, std::vector<const NFANode*>{to});
}

std::vector<const NFANode*> NFANode::get_transitions(std::uint16_t c) const {
    if (auto it = transitions_.find(c); it != transitions_.end())
        return it->second;
    else
        return {};
}

void NFANode::dump() const {
    auto print_char = [](std::uint16_t c) -> std::string {
        if (c == NFA::SpecialTransitons::EPSILON)
            return "ε";
        else if (c == NFA::SpecialTransitons::ANY)
            return ".";
        else
            return {static_cast<char>(c)};
    };

    if (this->is_accepting()) std::cout << "  \"" << this << "\" [shape=doublecircle];\n";

    for (auto& [c, tos] : transitions_)
        for (auto to : tos) std::cout << "  \"" << this << "\" -> \"" << to << "\" [label=\"" << print_char(c) << "\"];\n";
}

namespace thorin::automaton {
template class AutomatonBase<NFANode>;
}
