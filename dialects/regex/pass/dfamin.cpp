#include "dfamin.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>

#include "dialects/regex/pass/dfa.h"

using namespace thorin::automaton;

namespace {
#if 0
void printSet(const std::set<const DFANode*>& set) {
    std::cout << "{";
    for (auto state : set) std::cout << state << ", ";
    std::cout << "}\n";
}
#endif

// get reachable states
std::set<const DFANode*> get_reachable_states(const DFA& dfa) {
    std::set<const DFANode*> reachableStates;
    std::vector<const DFANode*> workList;
    workList.push_back(dfa.get_start());
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

std::set<const DFANode*> get_accepting_states(const std::set<const DFANode*>& reachableStates) {
    std::set<const DFANode*> acceptingStates;
    for (auto state : reachableStates)
        if (state->is_accepting()) acceptingStates.insert(state);
    return acceptingStates;
}

std::set<std::uint16_t> get_alphabet(const std::set<const DFANode*>& reachableStates) {
    std::set<std::uint16_t> alphabet;
    for (auto state : reachableStates) state->for_transitions([&](auto c, auto) { alphabet.insert(c); });
    return alphabet;
}

std::set<const DFANode*> operator-(const std::set<const DFANode*>& lhs, const std::set<const DFANode*>& rhs) {
    std::set<const DFANode*> result;
    for (auto state : lhs)
        if (!rhs.contains(state)) result.insert(state);
    return result;
}
std::set<const DFANode*> operator*(const std::set<const DFANode*>& lhs, const std::set<const DFANode*>& rhs) {
    std::set<const DFANode*> result;
    for (auto state : lhs)
        if (rhs.contains(state)) result.insert(state);
    return result;
}

std::vector<std::set<const DFANode*>> hopcroft(const std::set<const DFANode*>& reachableStates) {
    const auto alphabet = get_alphabet(reachableStates);

    const auto F = get_accepting_states(reachableStates);

    std::vector<std::set<const DFANode*>> P = {F, reachableStates - F};
    std::vector<std::set<const DFANode*>> W = {F, reachableStates - F};

    while (!W.empty()) {
#if 0
        std::cout << "P: ";
        for (const auto& S : P) printSet(S);
        std::cout << "W: ";
        for (const auto& S : W) printSet(S);
#endif
        auto A = W.back();
        W.pop_back();
        for (auto c : alphabet) {
            std::set<const DFANode*> X{};
            for (auto state : reachableStates) {
                state->for_transitions([&](auto c_, auto to) {
                    if (c_ == c && A.contains(to)) X.insert(state);
                });
            }
            for (auto Y : P) {
                auto YnX = Y * X;
                auto Y_X = Y - X;
                if (!YnX.empty() && !Y_X.empty()) {
                    P.erase(std::find(P.begin(), P.end(), Y));
                    P.push_back(YnX);
                    P.push_back(Y_X);
                    if (auto YWit = std::find(W.begin(), W.end(), Y); YWit != W.end()) {
                        W.erase(YWit);
                        W.push_back(YnX);
                        W.push_back(Y_X);
                    } else {
                        if (YnX.size() <= Y_X.size())
                            W.push_back(YnX);
                        else
                            W.push_back(Y_X);
                    }
                }
            }
        }
    }

    return P;
}

} // namespace

namespace thorin::automaton {
std::unique_ptr<DFA> minimize_dfa(const DFA& dfa) {
    const auto reachableStates = get_reachable_states(dfa);

    const auto P = hopcroft(reachableStates);

    auto minDfa = std::make_unique<DFA>();
    std::unordered_map<const DFANode*, DFANode*> dfaStates;
    for (auto& X : P) {
        auto state = minDfa->add_state();
        for (auto x : X) {
            if (x->is_accepting()) state->set_accepting(true);
            dfaStates.emplace(x, state);
        }
    }
    minDfa->set_start(dfaStates[dfa.get_start()]);
    for (auto& X : P) {
        auto state = dfaStates[*X.begin()];
        for (auto x : X) x->for_transitions([&](auto c, auto to) { state->add_transition(dfaStates[to], c); });
    }
    return minDfa;
}
} // namespace thorin::automaton
