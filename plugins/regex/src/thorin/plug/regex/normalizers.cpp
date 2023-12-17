#include <algorithm>
#include <iterator>
#include <numeric>
#include <ranges>
#include <vector>

#include <automaton/range_helper.h>
#include <fe/assert.h>

#include "thorin/axiom.h"
#include "thorin/def.h"
#include "thorin/tuple.h"
#include "thorin/world.h"

#include "thorin/util/log.h"

#include "thorin/plug/regex/regex.h"

namespace thorin::plug::regex {

template<quant id> Ref normalize_quant(Ref type, Ref callee, Ref arg) {
    auto& world = type->world();

    // quantifiers are idempotent
    if (thorin::match(id, arg)) return arg;

    if constexpr (id == quant::plus) {
        // (\d?)+ and (\d*)+ == \d*
        if (auto optional_app = thorin::match(quant::optional, arg))
            return world.call(quant::star, optional_app->arg());
        else if (auto star_app = thorin::match(quant::star, arg))
            return arg;
    } else if constexpr (id == quant::star) {
        // (\d?)* and (\d+)* == \d*
        if (auto quant_app = thorin::match<quant>(arg)) return world.app(callee, quant_app->arg());
    } else if constexpr (id == quant::optional) {
        // (\d*)? and (\d+)? == \d*
        if (auto star_app = thorin::match(quant::star, arg))
            return arg;
        else if (auto plus_app = thorin::match(quant::plus, arg))
            return world.call(quant::star, plus_app->arg());
    }

    return world.raw_app(type, callee, arg);
}

template<class ConjOrDisj> void flatten_in_arg(Ref arg, DefVec& new_args) {
    for (const auto* proj : arg->projs()) {
        // flatten conjs in conjs / disj in disjs
        if (auto seq_app = thorin::match<ConjOrDisj>(proj))
            flatten_in_arg<ConjOrDisj>(seq_app->arg(), new_args);
        else
            new_args.push_back(proj);
    }
}

template<class ConjOrDisj> DefVec flatten_in_arg(Ref arg) {
    DefVec new_args;
    flatten_in_arg<ConjOrDisj>(arg, new_args);
    return new_args;
}

template<class ConjOrDisj> Ref make_binary_tree(Ref type, Defs args) {
    assert(!args.empty());
    auto& world = args.front()->world();
    return std::accumulate(args.begin() + 1, args.end(), args.front(), [&type, &world](const Def* lhs, const Def* rhs) {
        return world.raw_app(type, world.call<ConjOrDisj>(world.lit_nat(2)), world.tuple({lhs, rhs}));
    });
}

Ref normalize_conj(Ref type, Ref callee, Ref arg) {
    auto& world = type->world();
    world.DLOG("conj {}:{} ({})", type, callee, arg);
    if (arg->as_lit_arity() > 2) {
        auto flat_args = flatten_in_arg<conj>(arg);
        return make_binary_tree<conj>(type, flat_args);
    }
    if (arg->as_lit_arity() > 1) return world.raw_app(type, callee, arg);
    return arg;
}

bool compare_re(const Def* lhs, const Def* rhs) {
    auto lhs_range = thorin::match<range>(lhs);
    auto rhs_range = thorin::match<range>(rhs);
    if (lhs_range) {
        // sort ranges by increasing lower bound
        if (rhs_range) return Lit::as(lhs_range->arg()->proj(0)) < Lit::as(rhs_range->arg()->proj(0));
        // ranges to the end
        return false;
    } else if (rhs_range) {
        // ranges to the end
        return true;
    }
    // keep
    return true;
}

void make_vector_unique(DefVec& args) {
    std::stable_sort(args.begin(), args.end(), &compare_re);
    {
        auto new_end = std::unique(args.begin(), args.end());
        args.erase(new_end, args.end());
    }
}

bool is_in_range(std::pair<nat_t, nat_t> range, nat_t needle) {
    return needle >= range.first && needle <= range.second;
}

auto get_range(const Def* rng) -> std::pair<nat_t, nat_t> {
    auto rng_match = thorin::match<range, false>(rng);
    return {Lit::as<std::uint8_t>(rng_match->arg(0)), Lit::as<std::uint8_t>(rng_match->arg(1))};
}

struct app_range {
    World& w;
    Ref operator()(std::pair<nat_t, nat_t> rng) {
        return w.call<range>(Defs{w.lit_int(8, rng.first), w.lit_int(8, rng.second)});
    }
};

void merge_ranges(DefVec& args) {
    auto ranges_begin = args.begin();
    while (ranges_begin != args.end() && !thorin::match<range>(*ranges_begin)) ranges_begin++;
    if (ranges_begin == args.end()) return;

    std::set<const Def*> to_remove;
    Vector<std::pair<nat_t, nat_t>> old_ranges;
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
    return (thorin::match(A, cls0) && thorin::match(B, cls1)) || (thorin::match(A, cls1) && thorin::match(B, cls0));
}

bool equals_any(const Def* lhs, const Def* rhs) {
    auto check_arg_equiv = [](const Def* lhs, const Def* rhs) {
        if (auto rng_lhs = thorin::match<range>(lhs))
            if (auto not_rhs = thorin::match<not_>(rhs)) {
                if (auto rng_rhs = thorin::match<range>(not_rhs->arg())) return rng_lhs == rng_rhs;
            }
        return false;
    };

    return check_arg_equiv(lhs, rhs) || check_arg_equiv(rhs, lhs);
}

bool equals_any(Defs lhs, Defs rhs) {
    Vector<std::pair<nat_t, nat_t>> lhs_ranges, rhs_ranges;
    auto only_ranges = std::ranges::views::filter([](auto d) { return match<range>(d); });
    std::ranges::transform(lhs | only_ranges, std::back_inserter(lhs_ranges), get_range);
    std::ranges::transform(rhs | only_ranges, std::back_inserter(rhs_ranges), get_range);
    return std::ranges::includes(lhs_ranges, rhs_ranges) || std::ranges::includes(rhs_ranges, lhs_ranges);
}

Ref normalize_disj(Ref type, Ref, Ref arg) {
    auto& world = type->world();
    if (arg->as_lit_arity() > 1) {
        auto contains_any = [](auto args) {
            return std::ranges::find_if(args, [](const Def* ax) -> bool { return thorin::match<any>(ax); })
                != args.end();
        };

        auto new_args = flatten_in_arg<disj>(arg);
        if (contains_any(new_args)) return world.annex<any>();
        make_vector_unique(new_args);
        merge_ranges(new_args);

        const Def* to_remove = nullptr;
        for (const auto* cls0 : new_args) {
            for (const auto* cls1 : new_args)
                if (equals_any(cls0, cls1)) return world.annex<any>();

            if (auto not_rhs = thorin::match<not_>(cls0)) {
                if (auto disj_rhs = thorin::match<disj>(not_rhs->arg())) {
                    auto rngs = flatten_in_arg<disj>(disj_rhs->arg());
                    make_vector_unique(rngs);
                    if (equals_any(new_args, rngs)) return world.annex<any>();
                }
            }
        }

        erase(new_args, to_remove);
        world.DLOG("final ranges {, }", new_args);

        const Def* retval = new_args.back();
        if (new_args.size() > 2)
            retval = make_binary_tree<disj>(type, new_args);
        else if (new_args.size() > 1)
            retval = world.raw_app(type, world.call<disj>(world.lit_nat(2)), new_args);
        world.DLOG("disj app: {}", retval);
        return retval;
    }
    return arg;
}

Ref normalize_range(Ref type, Ref callee, Ref arg) {
    auto& world     = type->world();
    auto [lhs, rhs] = arg->projs<2>();

    if (!lhs->isa<Var>() && !rhs->isa<Var>()) // before first PE.
        if (lhs->as<Lit>()->get() > rhs->as<Lit>()->get()) return world.raw_app(type, callee, world.tuple({rhs, lhs}));

    return world.raw_app(type, callee, arg);
}

Ref normalize_not(Ref type, Ref callee, Ref arg) {
    auto& world = type->world();
    return world.raw_app(type, callee, arg);
}

THORIN_regex_NORMALIZER_IMPL

} // namespace thorin::plug::regex
