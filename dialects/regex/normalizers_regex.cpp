#include <algorithm>
#include <iterator>
#include <numeric>
#include <vector>

#include <bits/ranges_util.h>

#include "thorin/axiom.h"
#include "thorin/tuple.h"
#include "thorin/world.h"

#include "thorin/util/assert.h"
#include "thorin/util/loc.h"
#include "thorin/util/log.h"

#include "dialects/regex/autogen.h"
#include "dialects/regex/regex.h"

namespace thorin::regex {

template<quant id>
Ref normalize_quant(Ref type, Ref callee, Ref arg) {
    auto& world = type->world();

    // quantifiers are idempotent
    if (thorin::match(id, arg)) return arg;

    if constexpr (id == quant::oneOrMore) {
        // (\d?)+ and (\d*)+ == \d*
        if (auto zeroOrOne_app = thorin::match(quant::zeroOrOne, arg))
            return world.app(world.annex<quant>(quant::zeroOrMore), zeroOrOne_app->arg());
        else if (auto zeroOrMore_app = thorin::match(quant::zeroOrMore, arg))
            return arg;
    } else if constexpr (id == quant::zeroOrMore) {
        // (\d?)* and (\d+)* == \d*
        if (auto quant_app = thorin::match<quant>(arg)) return world.app(callee, quant_app->arg());
    } else if constexpr (id == quant::zeroOrOne) {
        // (\d*)? and (\d+)? == \d*
        if (auto zeroOrMore_app = thorin::match(quant::zeroOrMore, arg))
            return arg;
        else if (auto oneOrMore_app = thorin::match(quant::oneOrMore, arg))
            return world.app(world.annex<quant>(quant::zeroOrMore), oneOrMore_app->arg());
    }

    return world.raw_app(type, callee, arg);
}

template<class ConjOrDisj>
std::vector<const Def*> flatten_in_arg(Ref arg) {
    std::vector<const Def*> newArgs;
    for (const auto* proj : arg->projs()) {
        // flatten conjs in conjs / disj in disjs
        if (auto seq_app = thorin::match<ConjOrDisj>(proj))
            for (auto seq_arg : seq_app->args()) newArgs.push_back(seq_arg);
        else
            newArgs.push_back(proj);
    }
    return newArgs;
}

template<class ConjOrDisj>
Ref make_binary_tree(Ref type, DefArray args) {
    assert(!args.empty());
    auto& world = args.front()->world();
    return std::accumulate(args.begin() + 1, args.end(), args.front(), [&type, &world](const Def* lhs, const Def* rhs) {
        return world.raw_app(type, world.app(world.annex<ConjOrDisj>(), world.lit_nat(2)), world.tuple({lhs, rhs}));
    });
}

Ref normalize_conj(Ref type, Ref callee, Ref arg) {
    auto& world = type->world();
    world.DLOG("conj {}:{} ({})", type, callee, arg);
    if (arg->as_lit_arity() > 2) {
        auto flatArgs = flatten_in_arg<conj>(arg);
        return make_binary_tree<conj>(type, flatArgs);
    }
    if (arg->as_lit_arity() > 1) return world.raw_app(type, callee, arg);
    return arg;
}

template<cls A, cls B>
bool equals_any(const Def* cls0, const Def* cls1) {
    return (thorin::match(A, cls0) && thorin::match(B, cls1)) || (thorin::match(A, cls1) && thorin::match(B, cls0));
}

bool equals_any(const Def* lhs, const Def* rhs) {
    auto check_arg_equiv = [](const Def* lhs, const Def* rhs) {
        if (auto rng_lhs = thorin::match<range>(lhs))
            if (auto not_rhs = thorin::match<not_>(rhs))
                if (auto rng_rhs = thorin::match<range>(not_rhs->arg())) return rng_lhs == rng_rhs;
        return false;
    };

    return check_arg_equiv(lhs, rhs) || check_arg_equiv(rhs, lhs);
}

bool compare_re(const Def* lhs, const Def* rhs) {
    auto lhs_range = thorin::match<range>(lhs);
    auto rhs_range = thorin::match<range>(rhs);
    if (lhs_range) {
        // sort ranges by increasing lower bound
        if (rhs_range) return Lit::as(rhs_range->arg()->proj(0)) < Lit::as(rhs_range->arg()->proj(0));
        // ranges just before the lits
        return false;
    } else if (rhs_range) {
        // ranges just before the lits
        return true;
    }
    return true;
}

void make_vector_unique(std::vector<const Def*>& args) {
    std::stable_sort(args.begin(), args.end(), &compare_re);
    {
        auto newEnd = std::unique(args.begin(), args.end());
        args.erase(newEnd, args.end());
    }
}

std::optional<std::pair<nat_t, nat_t>> merge_ranges(std::pair<nat_t, nat_t> a, std::pair<nat_t, nat_t> b) {
    if (!(a.second + 1 < b.first || b.second + 1 < a.first)) {
        return {
            {std::min(a.first, b.first), std::max(a.second, b.second)}
        };
    }
    return {};
    return {};
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
        return w.app(w.annex<range>(), w.tuple({w.lit_int(8, rng.first), w.lit_int(8, rng.second)}));
    }
};

void merge_ranges(std::vector<const Def*>& args) {
    auto rangesBegin = args.begin();
    while (rangesBegin != args.end() && !thorin::match<range>(*rangesBegin)) rangesBegin++;
    if (rangesBegin == args.end()) return;

    std::set<const Def*> toRemove;
    std::vector<std::pair<nat_t, nat_t>> oldRanges, newRanges;
    auto& world = (*rangesBegin)->world();

    std::transform(rangesBegin, args.end(), std::back_inserter(oldRanges), get_range);

    for (auto it = oldRanges.begin(); it != oldRanges.end(); ++it) {
        auto current_range = *it;
        for (auto inner = it + 1; inner != oldRanges.end(); ++inner)
            if (auto merged = merge_ranges(current_range, *inner)) current_range = *merged;

        std::vector<std::vector<std::pair<nat_t, nat_t>>::iterator> deDuplicate;
        for (auto inner = newRanges.begin(); inner != newRanges.end(); ++inner) {
            if (auto merged = merge_ranges(current_range, *inner)) {
                current_range = *merged;
                deDuplicate.push_back(inner);
            }
        }
        for (auto dedup : deDuplicate) newRanges.erase(dedup);
        newRanges.push_back(std::move(current_range));
    }
    // invalidates rangesBegin
    args.erase(rangesBegin, args.end());
    std::transform(newRanges.begin(), newRanges.end(), std::back_inserter(args), app_range{world});

    make_vector_unique(args);
}

Ref normalize_disj(Ref type, Ref, Ref arg) {
    auto& world = type->world();
    if (arg->as_lit_arity() > 1) {
        auto contains_any = [](auto args) {
            return std::ranges::find_if(args, [](const Def* ax) -> bool { return thorin::match(cls::any, ax); })
                != args.end();
        };

        auto newArgs = flatten_in_arg<disj>(arg);
        if (contains_any(newArgs)) return world.annex(cls::any);
        make_vector_unique(newArgs);
        merge_ranges(newArgs);

        const Def* toRemove = nullptr;
        for (const auto* cls0 : newArgs)
            for (const auto* cls1 : newArgs)
                if (equals_any(cls0, cls1)) return world.annex(cls::any);

        std::erase(newArgs, toRemove);

        if (newArgs.size() > 2) return make_binary_tree<disj>(type, newArgs);
        if (newArgs.size() > 1)
            return world.raw_app(type, world.app(world.annex<disj>(), world.lit_nat(2)), world.tuple(newArgs));
        return newArgs.back();
    }
    return arg;
}

Ref normalize_group(Ref type, Ref callee, Ref arg) {
    auto& world = type->world();
    if (arg->as_lit_arity() > 1) {
        auto&& newArgs = flatten_in_arg<conj>(arg);
        return world.raw_app(type, world.app(world.annex<group>(), world.lit_nat(newArgs.size())),
                             world.tuple(newArgs));
    }
    return world.raw_app(type, callee, arg);
}

template<cls id>
Ref normalize_cls(Ref type, Ref callee, Ref arg) {
    auto& world   = type->world();
    auto make     = app_range{world};
    auto make_not = [&](Ref arg) { return world.app(world.annex<not_>(), arg); };
    auto make_w   = [&]() {
        return world.app(world.app(world.annex<disj>(), world.lit_nat(3)),
                           world.tuple({make({'0', '9'}), make({'a', 'z'}), make({'A', 'Z'}), make({'_', '_'})}));
    };
    auto make_s = [&]() {
        return world.app(world.app(world.annex<disj>(), world.lit_nat(3)),
                         world.tuple({make({9, 10}), make({13, 13}), make({32, 32})}));
    };
    switch (thorin::match<cls>(callee).id()) {
        case cls::d: return make({'0', '9'});
        case cls::D: return make_not(make({'0', '9'}));
        case cls::w: return make_w();
        case cls::W: return make_not(make_w());
        case cls::s: return make_s();
        case cls::S: return make_not(make_s());
        case cls::any: return world.raw_app(type, callee, arg);
    }
    unreachable();
}

Ref normalize_lit(Ref type, Ref, Ref arg) {
    auto& world = type->world();

    return world.app(world.annex<range>(), world.tuple({arg, arg}));
}

Ref normalize_range(Ref type, Ref callee, Ref arg) {
    auto& world     = type->world();
    auto [lhs, rhs] = arg->projs<2>();

    if (lhs->as<Lit>()->get() > rhs->as<Lit>()->get())
        error(arg->dbg().loc, "%regex.range parameters in broken order: {} must be smaller than {}", lhs, rhs);

    return world.raw_app(type, callee, arg);
}

Ref normalize_not(Ref type, Ref callee, Ref arg) {
    auto& world = type->world();
    return world.raw_app(type, callee, arg);
}

THORIN_regex_NORMALIZER_IMPL

} // namespace thorin::regex
