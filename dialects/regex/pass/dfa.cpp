#include "dfa.h"

#include <iostream>

#include "thorin/util/util.h"

#include "dialects/regex/pass/automaton.h"

using namespace thorin::automaton;

void DFANode::add_transition(const DFANode* to, std::uint16_t c) { transitions_[c] = to; }

const DFANode* DFANode::get_transition(std::uint16_t c) const { return lookup(transitions_, c); }

void DFANode::dump() const {
    auto print_char = [](std::uint16_t c) -> std::string {
        if (c == DFA::SpecialTransitons::ANY)
            return ".";
        else
            return {static_cast<char>(c)};
    };

    if (this->is_accepting()) std::cout << "  \"" << this << "\" [shape=doublecircle];\n";

    for (auto& [c, to] : transitions_)
        std::cout << "  \"" << this << "\" -> \"" << to << "\" [label=\"" << print_char(c) << "\"];\n";
}

namespace thorin::automaton {
template class AutomatonBase<DFANode>;
}
