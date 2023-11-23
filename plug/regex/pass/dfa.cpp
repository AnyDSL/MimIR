#include "dfa.h"

#include <iostream>

#include "thorin/def.h"

#include "thorin/util/util.h"

#include "absl/container/flat_hash_map.h"
#include "plug/regex/pass/automaton.h"
#include "plug/regex/range_helper.h"

using namespace thorin::automaton;

void DFANode::add_transition(const DFANode* to, std::uint16_t c) { transitions_[c] = to; }

const DFANode* DFANode::get_transition(std::uint16_t c) const {
    if (erroring_) return nullptr;
    return lookup(transitions_, c);
}

namespace thorin::automaton {
template class AutomatonBase<DFANode>;

std::ostream& operator<<(std::ostream& os, const DFANode& node) {
    auto print_char = [](std::uint16_t c) -> std::string {
        if (c >= 48 && c <= 122) return {static_cast<char>(c)};
        return std::to_string(c);
    };
    return print_node(os, node, print_char);
}
} // namespace thorin::automaton
