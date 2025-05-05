#include <algorithm>
#include <iterator>
#include <numeric>
#include <ranges>
#include <vector>

#include <automaton/range_helper.h>
#include <fe/assert.h>

#include "mim/axm.h"
#include "mim/def.h"
#include "mim/tuple.h"
#include "mim/world.h"

#include "mim/util/log.h"

#include "mim/plug/regex/regex.h"

using Range  = automaton::Range;
using Ranges = mim::Vector<Range>;

namespace mim::plug::regex {

template<quant id> const Def* normalize_quant(const Def* type, const Def* callee, const Def* arg) {
    auto& world = type->world();

    // quantifiers are idempotent
    if (Axm::isa(id, arg)) return arg;

    if constexpr (id == quant::plus) {
        // (\d?)+ and (\d*)+ == \d*
        if (auto optional_app = Axm::isa(quant::optional, arg))
            return world.call(quant::star, optional_app->arg());
        else if (auto star_app = Axm::isa(quant::star, arg))
            return arg;
    } else if constexpr (id == quant::star) {
        // (\d?)* and (\d+)* == \d*
        if (auto quant_app = Axm::isa<quant>(arg)) return world.app(callee, quant_app->arg());
    } else if constexpr (id == quant::optional) {
        // (\d*)? and (\d+)? == \d*
        if (auto star_app = Axm::isa(quant::star, arg))
            return arg;
        else if (auto plus_app = Axm::isa(quant::plus, arg))
            return world.call(quant::star, plus_app->arg());
    }

    return {};
}

template<class ConjOrDisj> void flatten_in_arg(const Def* arg, DefVec& new_args) {
    for (const auto* proj : arg->projs()) {
        // flatten conjs in conjs / disj in disjs
        if (auto seq_app = Axm::isa<ConjOrDisj>(proj))
            flatten_in_arg<ConjOrDisj>(seq_app->arg(), new_args);
        else
            new_args.push_back(proj);
    }
}

template<class ConjOrDisj> DefVec flatten_in_arg(const Def* arg) {
    DefVec new_args;
    flatten_in_arg<ConjOrDisj>(arg, new_args);
    return new_args;
}

template<class ConjOrDisj> const Def* make_binary_tree(Defs args) {
    assert(!args.empty());
    auto& world = args.front()->world();
    return std::accumulate(args.begin() + 1, args.end(), args.front(), [&world](const Def* lhs, const Def* rhs) {
        return world.call<ConjOrDisj, false>(Defs{lhs, rhs});
    });
}

const Def* normalize_conj(const Def* type, const Def* callee, const Def* arg) {
    auto& world = type->world();
    world.DLOG("conj {}:{} ({})", type, callee, arg);
    if (arg->as_lit_arity() > 2) {
        auto flat_args = flatten_in_arg<conj>(arg);
        return make_binary_tree<conj>(flat_args);
    }

    return arg->as_lit_arity() == 1 ? arg : nullptr;
}

bool compare_re(const Def* lhs, const Def* rhs) {
    auto lhs_range = Axm::isa<range>(lhs);
    auto rhs_range = Axm::isa<range>(rhs);
    // sort ranges by increasing lower bound
    if (lhs_range && rhs_range) return Lit::as(lhs_range->arg()->proj(0)) < Lit::as(rhs_range->arg()->proj(0));
    // ranges to the end
    if (lhs_range) return false;
    if (rhs_range) return true;

    return lhs->gid() < rhs->gid(); // make irreflexive
}

void make_vector_unique(DefVec& args) {
    std::stable_sort(args.begin(), args.end(), &compare_re);
    {
        auto new_end = std::unique(args.begin(), args.end());
        args.erase(new_end, args.end());
    }
}

bool is_in_range(Range range, nat_t needle) { return needle >= range.first && needle <= range.second; }

auto get_range(const Def* rng) -> Range {
    auto rng_match = Axm::isa<range, false>(rng);
    return {Lit::as<std::uint8_t>(rng_match->arg(0)), Lit::as<std::uint8_t>(rng_match->arg(1))};
}

struct app_range {
    World& w;
    const Def* operator()(Range rng) { return w.call<range>(Defs{w.lit_i8(rng.first), w.lit_i8(rng.second)}); }
};

void merge_ranges(DefVec& args) {
    auto ranges_begin = args.begin();
    while (ranges_begin != args.end() && !Axm::isa<range>(*ranges_begin)) ranges_begin++;
    if (ranges_begin == args.end()) return;

    std::set<const Def*> to_remove;
    Ranges old_ranges;
    auto& world = (*ranges_begin)->world();

    std::transform(ranges_begin, args.end(), std::back_inserter(old_ranges), get_range);

    auto new_ranges = automaton::merge_ranges(
        old_ranges, [&world](auto&&... args) { world.DLOG(std::forward<decltype(args)>(args)...); });

    // invalidates ranges_begin
    args.erase(ranges_begin, args.end());
    std::transform(new_ranges.begin(), new_ranges.end(), std::back_inserter(args), app_range{world});

    make_vector_unique(args);
}

template<cls A, cls B> bool equals_any(const Def* cls0, const Def* cls1) {
    return (Axm::isa(A, cls0) && Axm::isa(B, cls1)) || (Axm::isa(A, cls1) && Axm::isa(B, cls0));
}

bool equals_any(const Def* lhs, const Def* rhs) {
    auto check_arg_equiv = [](const Def* lhs, const Def* rhs) {
        if (auto rng_lhs = Axm::isa<range>(lhs))
            if (auto not_rhs = Axm::isa<not_>(rhs)) {
                if (auto rng_rhs = Axm::isa<range>(not_rhs->arg())) return rng_lhs == rng_rhs;
            }
        return false;
    };

    return check_arg_equiv(lhs, rhs) || check_arg_equiv(rhs, lhs);
}

bool equals_any(Defs lhs, Defs rhs) {
    Ranges lhs_ranges, rhs_ranges;
    auto only_ranges = std::ranges::views::filter([](auto d) { return Axm::isa<range>(d); });
    std::ranges::transform(lhs | only_ranges, std::back_inserter(lhs_ranges), get_range);
    std::ranges::transform(rhs | only_ranges, std::back_inserter(rhs_ranges), get_range);
    return std::ranges::includes(lhs_ranges, rhs_ranges) || std::ranges::includes(rhs_ranges, lhs_ranges);
}

const Def* normalize_disj(const Def* type, const Def*, const Def* arg) {
    auto& world = type->world();
    if (arg->as_lit_arity() > 1) {
        auto contains_any = [](auto args) {
            return std::ranges::find_if(args, [](const Def* ax) -> bool { return Axm::isa<any>(ax); }) != args.end();
        };

        auto new_args = flatten_in_arg<disj>(arg);
        if (contains_any(new_args)) return world.annex<any>();
        make_vector_unique(new_args);
        merge_ranges(new_args);

        const Def* to_remove = nullptr;
        for (const auto* cls0 : new_args) {
            for (const auto* cls1 : new_args)
                if (equals_any(cls0, cls1)) return world.annex<any>();

            if (auto not_rhs = Axm::isa<not_>(cls0)) {
                if (auto disj_rhs = Axm::isa<disj>(not_rhs->arg())) {
                    auto rngs = flatten_in_arg<disj>(disj_rhs->arg());
                    make_vector_unique(rngs);
                    if (equals_any(new_args, rngs)) return world.annex<any>();
                }
            }
        }

        erase(new_args, to_remove);
        world.DLOG("final ranges {, }", new_args);

        if (new_args.size() > 2) return make_binary_tree<disj>(new_args);
        if (new_args.size() > 1) return world.call<disj, false>(new_args);
        return new_args.back();
    }
    return arg;
}

const Def* normalize_range(const Def* type, const Def* callee, const Def* arg) {
    auto& world     = type->world();
    auto [lhs, rhs] = arg->projs<2>();

    if (!lhs->isa<Var>() && !rhs->isa<Var>()) // before first PE.
        if (lhs->as<Lit>()->get() > rhs->as<Lit>()->get()) return world.raw_app(type, callee, {rhs, lhs});

    return {};
}

const Def* normalize_not(const Def*, const Def*, const Def*) { return {}; }

MIM_regex_NORMALIZER_IMPL

} // namespace mim::plug::regex
