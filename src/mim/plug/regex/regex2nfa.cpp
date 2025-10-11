#include "mim/plug/regex/regex2nfa.h"

#include <algorithm>

#include <automaton/nfa.h>

#include <mim/lam.h>
#include <mim/world.h>

#include <mim/plug/regex/regex.h>

using namespace mim;

namespace mim::plug::regex {

namespace {
struct Regex2NfaConverter {
    Regex2NfaConverter()
        : nfa_(std::make_unique<automaton::NFA>()) {}

    void add_range_transitions(automaton::NFANode* from, automaton::NFANode* to, std::uint16_t lb, std::uint16_t ub) {
        for (auto i = lb; i <= ub; ++i)
            from->add_transition(to, i);
    }

    void add_range_transitions(automaton::NFANode* from, automaton::NFANode* to, const Def* lit0, const Def* lit1) {
        auto lb = lit0->as<Lit>()->get();
        auto ub = lit1->as<Lit>()->get();
        add_range_transitions(from, to, lb, ub);
    }

    void
    convert(const Def* regex, automaton::NFANode* start, automaton::NFANode* end, automaton::NFANode* error = nullptr) {
        if (auto conj = Axm::isa<regex::conj>(regex)) {
            auto middle = nfa_->add_state();
            convert(conj->arg(0), start, middle, error);
            convert(conj->arg(1), middle, end, error);
        } else if (auto any = Axm::isa<regex::any>(regex)) {
            add_range_transitions(start, end, 0_u16, 255);
        } else if (auto range = Axm::isa<regex::range>(regex)) {
            add_range_transitions(start, end, range->arg(0), range->arg(1));
        } else if (auto not_ = Axm::isa<regex::not_>(regex)) {
            auto first = nfa_->add_state();
            auto error = nfa_->add_state();
            // error->set_erroring(true);

            start->add_transition(first, automaton::NFA::SpecialTransitons::EPSILON);
            auto args = detail::flatten_in_arg<regex::disj>(not_->arg());
            std::vector<std::pair<std::uint8_t, std::uint8_t>> ranges;
            for (auto& arg : args) {
                assert(Axm::isa<regex::range>(arg)
                       && "as per normalizer, if we're in a 'not_' argument, we must only have disjs and ranges!");

                auto rng_match = Axm::isa<regex::range, false>(arg);
                ranges.emplace_back(Lit::as<std::uint8_t>(rng_match->arg(0)), Lit::as<std::uint8_t>(rng_match->arg(1)));
            }
            std::sort(ranges.begin(), ranges.end());
            std::uint8_t last{0};
            for (auto rng : ranges) {
                if (rng.first > last) add_range_transitions(first, end, last, rng.first - 1);
                add_range_transitions(first, error, rng.first, rng.second);
                last = std::min(rng.second + 1_u16, 255);
            }
            if (last < 255) add_range_transitions(first, end, last, 255);
        } else if (auto disj = Axm::isa<regex::disj>(regex)) {
            convert(disj->arg(0), start, end, error);
            convert(disj->arg(1), start, end, error);
        } else if (auto opt = Axm::isa(quant::optional, regex)) {
            start->add_transition(end, automaton::NFA::SpecialTransitons::EPSILON);
            convert(opt->arg(), start, end);
        } else if (auto star = Axm::isa(quant::star, regex)) {
            auto m1 = nfa_->add_state();
            auto m2 = nfa_->add_state();
            start->add_transition(m1, automaton::NFA::SpecialTransitons::EPSILON);
            start->add_transition(end, automaton::NFA::SpecialTransitons::EPSILON);
            m2->add_transition(m1, automaton::NFA::SpecialTransitons::EPSILON);
            m2->add_transition(end, automaton::NFA::SpecialTransitons::EPSILON);
            convert(star->arg(), m1, m2);
        } else if (auto plus = Axm::isa(quant::plus, regex)) {
            auto m0 = nfa_->add_state();
            auto m1 = nfa_->add_state();
            auto m2 = nfa_->add_state();
            convert(plus->arg(), start, m0, error);
            m0->add_transition(m1, automaton::NFA::SpecialTransitons::EPSILON);
            m2->add_transition(m1, automaton::NFA::SpecialTransitons::EPSILON);
            m2->add_transition(end, automaton::NFA::SpecialTransitons::EPSILON);
            m0->add_transition(end, automaton::NFA::SpecialTransitons::EPSILON);
            convert(plus->arg(), m1, m2);
        } else if (auto empty = Axm::isa<regex::empty>(regex)) {
            start->add_transition(end, automaton::NFA::SpecialTransitons::EPSILON);
        }
    }

    void convert(const Def* regex) {
        auto start = nfa_->add_state();
        nfa_->set_start(start);
        auto end = nfa_->add_state();
        end->set_accepting(true);
        convert(regex, start, end);
    }

    std::unique_ptr<automaton::NFA> nfa() { return std::move(nfa_); }

private:
    std::unique_ptr<automaton::NFA> nfa_;
};

} // namespace

std::unique_ptr<automaton::NFA> regex2nfa(const Def* regex) {
    Regex2NfaConverter converter;
    converter.convert(regex);
    return converter.nfa();
}

} // namespace mim::plug::regex

extern "C" automaton::NFA* regex2nfa(const Def* regex) { return mim::plug::regex::regex2nfa(regex).release(); }
