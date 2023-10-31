#include "dfa2matcher.h"

#include <algorithm>
#include <sstream>

#include <absl/container/flat_hash_map.h>

#include "thorin/def.h"

#include "dialects/core/core.h"
#include "dialects/mem/mem.h"
#include "dialects/regex/pass/dfa.h"
#include "dialects/regex/range_helper.h"

using namespace thorin;

// nomenclature:
// c is the character from the string we want to match

// see lit/regex/match_manual.thorin for a hand written state machine impl that this is based on

// the main idea is:
// for every state in the DFA, we create a lam that checks if the char c at pos is the end of the string \0
// if not, jump to a checker lam, that consists of a few bit-wise or-ed in-range checks to verify if we can transition
// to a certain other state with c if any of the checks is true, we jump to the corresponding state lam with the updated
// position if the check fails, we jump to the next checker lam or the exit lam if we checked all possible transitions
// Note, since the DFA stores the transitions as chars, not ranges, we have to merge the transitions to ranges first
// (cf. transitions_to_ranges)

#define DEBUG_PRINT 0

namespace {
std::string state_to_name(const automaton::DFANode* state) {
    std::stringstream ss;
    ss << "state_" << state;
    return ss.str();
}

// std::vector<std::pair<nat_t, nat_t>> merge_ranges(World& w, const std::vector<std::pair<nat_t, nat_t>>& old_ranges);
absl::flat_hash_map<const automaton::DFANode*, std::vector<std::pair<nat_t, nat_t>>>
transitions_to_ranges(World& w, const automaton::DFANode* state) {
    absl::flat_hash_map<const automaton::DFANode*, std::vector<std::pair<nat_t, nat_t>>> state2ranges;
    state->for_transitions([&](std::uint16_t transition, const automaton::DFANode* next_state) {
        if (!state2ranges.contains(next_state))
            state2ranges.emplace(next_state, std::vector<std::pair<nat_t, nat_t>>{
                                                 {transition, transition}
            });
        else
            state2ranges[next_state].emplace_back(transition, transition);
    });
    std::pair<nat_t, nat_t> any_range{automaton::DFA::SpecialTransitons::ANY, automaton::DFA::SpecialTransitons::ANY};
    for (auto& [state, ranges] : state2ranges) {
        if (std::find(ranges.cbegin(), ranges.cend(), any_range) != ranges.cend()) {
            ranges = {any_range};
            continue;
        }

        std::sort(ranges.begin(), ranges.end(), [](auto a, auto b) { return a.first < b.first; });
        ranges = regex::merge_ranges(w, ranges);
    }
    return state2ranges;
}

Ref match_range(Ref c, nat_t lo, nat_t hi) {
    World& w = c->world();
    if (lo == automaton::DFA::SpecialTransitons::ANY) return w.lit_tt();

    // .let in_range     = %core.bit2.and_ 0 (%core.icmp.uge (char, lower),  %core.icmp.ule (char, upper));
    auto below_hi = w.call(core::icmp::ule, w.tuple({c, w.lit_idx(256, hi)}));
    auto above_lo = w.call(core::icmp::uge, w.tuple({c, w.lit_idx(256, lo)}));
    return w.call(core::bit2::and_, w.lit_nat(2), w.tuple({below_hi, above_lo}));
}

absl::flat_hash_map<const automaton::DFANode*, Ref>
create_check_match_transitions_from(Ref c, const automaton::DFANode* state) {
    World& w = c->world();
    absl::flat_hash_map<const automaton::DFANode*, Ref> state2check;

    auto state2ranges = transitions_to_ranges(w, state);

    for (auto& [state, ranges] : state2ranges) {
        for (auto& [lo, hi] : ranges)
            if (!state2check.contains(state))
                state2check.emplace(state, match_range(c, lo, hi));
            else
                state2check[state]
                    = w.call(core::bit2::or_, w.lit_nat(2), w.tuple({state2check[state], match_range(c, lo, hi)}));
    }
    return state2check;
}

} // namespace

namespace thorin::regex {
Ref dfa2matcher(World& w, const automaton::DFA& dfa, Ref n) {
#if DEBUG_PRINT
    dfa.dump("dfa_to_match");
#endif
    auto states = dfa.get_reachable_states();
    absl::flat_hash_map<const automaton::DFANode*, Lam*> state2matcher;

    // ((mem: %mem.M, string: Str n, pos: .Idx n), .Cn [%mem.M, .Bool, .Idx n])
    auto matcher_con  = w.cn(w.sigma({w.annex<mem::M>(), w.type_bool(), w.type_idx(n)}));
    auto matcher_args = w.sigma({w.annex<mem::M>(), w.call<mem::Ptr0>(w.arr(n, w.type_idx(256))), w.type_idx(n)});
    auto matcher_dom  = w.cn(w.sigma({matcher_args, matcher_con}));
    auto matcher      = w.mut_lam(matcher_dom);
    matcher->debug_prefix(std::string("match_regex"));
    auto [args, exit] = matcher->vars<2>();
    exit->debug_prefix(std::string("exit"));
    auto [mem, string, pos] = args->projs<3>();
    mem->debug_prefix(std::string("mem"));
    string->debug_prefix(std::string("string"));
    pos->debug_prefix(std::string("pos"));

    auto error = w.mut_lam(w.cn(w.sigma({w.annex<mem::M>(), w.type_idx(n)})));
    error->debug_prefix("error");
    {
        auto [mem, pos] = error->vars<2>();
        mem->debug_prefix(std::string("mem"));
        pos->debug_prefix(std::string("pos"));
        error->app(false, exit, {mem, w.lit_ff(), pos});
    }

    auto accept = w.mut_lam(w.cn(w.sigma({w.annex<mem::M>(), w.type_idx(n)})));
    accept->debug_prefix("accept");
    {
        auto [mem, pos] = accept->vars<2>();
        mem->debug_prefix(std::string("mem"));
        pos->debug_prefix(std::string("pos"));
        accept->app(false, exit, {mem, w.lit_tt(), pos});
    }

    auto exiting = [error, accept](const automaton::DFANode* state) { return state->is_accepting() ? accept : error; };

    const auto Pi = w.cn(w.sigma({w.annex<mem::M>(), w.type_idx(n)}));
    for (auto state : states) {
        auto lam = w.mut_lam(Pi);
        lam->debug_prefix(state_to_name(state));
        state2matcher.emplace(state, lam);
    }

    for (auto [state, lam] : state2matcher) {
        auto [mem, i]  = lam->vars<2>();
        auto lea       = w.call<mem::lea>(w.tuple({n, w.pack(n, w.type_idx(256)), w.lit_nat(0)}), w.tuple({string, i}));
        auto [mem2, c] = w.call<mem::load>(w.tuple({mem, lea}))->projs<2>();

        auto is_end  = w.call(core::icmp::e, w.tuple({c, w.lit_idx(256, 0)}));
        auto not_end = w.mut_lam(Pi);
        not_end->debug_prefix("not_end_" + state_to_name(state));

        auto new_i
            = w.call(core::wrap::add, core::Mode::nusw, w.tuple({i, w.call(core::conv::u, n, w.lit_int(64, 1))}));
        lam->app(false, w.select(is_end, exiting(state), not_end), {mem2, i});

        auto transitions = create_check_match_transitions_from(c, state);
        auto next_check  = exiting(state); // if we want to check full string only, use error instead of exiting(state)c
        for (auto [next_state, check] : transitions) {
            auto next_lam = state2matcher[next_state];
            auto checker  = w.mut_lam(Pi);
            checker->debug_prefix("check_" + state_to_name(state) + "_to_" + state_to_name(next_state));
            auto [mem3, pos] = checker->vars<2>();
            checker->app(false, w.select(check, next_lam, next_check), {mem3, w.select(check, new_i, pos)});
            next_check = checker;
        }
        {
            auto [mem, pos] = not_end->vars<2>();
            not_end->app(true, next_check, {mem, pos});
        }
    }

    matcher->app(false, state2matcher[dfa.get_start()], {mem, pos});
    return matcher;
}

} // namespace thorin::regex
