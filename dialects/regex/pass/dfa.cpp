#include "dfa.h"

#include <iostream>

#include "thorin/util/util.h"

#include "dialects/regex/pass/automaton.h"

using namespace thorin::automaton;

void DFANode::add_transition(const DFANode* to, std::uint16_t c) { transitions_[c] = to; }

const DFANode* DFANode::get_transition(std::uint16_t c) const { return lookup(transitions_, c); }

namespace thorin::automaton {
template class AutomatonBase<DFANode>;

std::ostream& operator<<(std::ostream& os, const DFANode& node) {
    auto print_char = [](std::uint16_t c) -> std::string {
        if (c == DFA::SpecialTransitons::ANY)
            return ".";
        else
            return {static_cast<char>(c)};
    };

    if (node.is_accepting()) os << "  \"" << &node << "\" [shape=doublecircle];\n";

    for (auto& [c, to] : node.transitions_)
        std::cout << "  \"" << &node << "\" -> \"" << to << "\" [label=\"" << print_char(c) << "\"];\n";
    return os;
}
} // namespace thorin::automaton
