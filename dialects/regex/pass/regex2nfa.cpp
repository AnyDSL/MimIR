#include "regex2nfa.h"

#include "thorin/lam.h"
#include "thorin/world.h"

#include "dialects/regex/pass/nfa.h"

using namespace thorin;

namespace thorin::regex {

namespace {
struct Regex2NfaConverter {
    Regex2NfaConverter()
        : nfa_(std::make_unique<automaton::NFA>()) {}

    void add_range_transitions(automaton::NFANode* from, automaton::NFANode* to, Ref lit0, Ref lit1) {
        auto lb = lit0->as<Lit>()->get();
        auto ub = lit1->as<Lit>()->get();

        for (auto i = lb; i <= ub; ++i) from->add_transition(to, i);
    }

    void convert(Ref regex, automaton::NFANode* start, automaton::NFANode* end) {
        if (auto conj = thorin::match<regex::conj>(regex)) {
            auto middle = nfa_->add_state();
            convert(conj->arg(0), start, middle);
            convert(conj->arg(1), middle, end);
        } else if (auto any = thorin::match<regex::any>(regex)) {
            start->add_transition(end, automaton::NFA::SpecialTransitons::ANY);
        } else if (auto range = thorin::match<regex::range>(regex)) {
            add_range_transitions(start, end, range->arg(0), range->arg(1));
        } else if (auto not_ = thorin::match<regex::not_>(regex)) {
            // TODO: implement
        } else if (auto disj = thorin::match<regex::disj>(regex)) {
            convert(disj->arg(0), start, end);
            convert(disj->arg(1), start, end);
        } else if (auto opt = thorin::match(quant::optional, regex)) {
            start->add_transition(end, automaton::NFA::SpecialTransitons::EPSILON);
            convert(opt->arg(), start, end);
        } else if (auto star = thorin::match(quant::star, regex)) {
            auto m1 = nfa_->add_state();
            auto m2 = nfa_->add_state();
            start->add_transition(m1, automaton::NFA::SpecialTransitons::EPSILON);
            start->add_transition(end, automaton::NFA::SpecialTransitons::EPSILON);
            m2->add_transition(m1, automaton::NFA::SpecialTransitons::EPSILON);
            m2->add_transition(end, automaton::NFA::SpecialTransitons::EPSILON);
            convert(star->arg(), m1, m2);
        } else if (auto plus = thorin::match(quant::plus, regex)) {
            auto m0 = nfa_->add_state();
            auto m1 = nfa_->add_state();
            auto m2 = nfa_->add_state();
            convert(plus->arg(), start, m0);
            m0->add_transition(m1, automaton::NFA::SpecialTransitons::EPSILON);
            m2->add_transition(m1, automaton::NFA::SpecialTransitons::EPSILON);
            m2->add_transition(end, automaton::NFA::SpecialTransitons::EPSILON);
            m0->add_transition(end, automaton::NFA::SpecialTransitons::EPSILON);
            convert(plus->arg(), m1, m2);
        }
    }

    void convert(Ref regex) {
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

std::unique_ptr<automaton::NFA> regex2nfa(Ref regex) {
    Regex2NfaConverter converter;
    converter.convert(regex);
    return converter.nfa();
}

} // namespace thorin::regex
