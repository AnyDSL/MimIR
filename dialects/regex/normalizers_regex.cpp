#include <algorithm>
#include <iterator>
#include <numeric>
#include <vector>

#include <bits/ranges_util.h>

#include "thorin/axiom.h"
#include "thorin/tuple.h"
#include "thorin/world.h"

#include "thorin/util/assert.h"
#include "thorin/util/log.h"

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

bool compare_re(const Def* lhs, const Def* rhs) {
    auto lhs_lit = thorin::match<lit>(lhs);
    auto rhs_lit = thorin::match<lit>(rhs);
    if (lhs_lit) {
        if (rhs_lit) return Lit::as(lhs_lit->arg()) < Lit::as(rhs_lit->arg());
        // lits at the end
        return false;
    } else if (rhs_lit) {
        // lits at the end
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

void reduceLitsToClass(std::vector<const Def*>& args) {
    bool has_cls_d = false, has_cls_w = false, has_cls_s = false;
    bool has_cls_D = false, has_cls_W = false, has_cls_S = false;

    auto litBegin = args.begin();
    while (litBegin != args.end() && !thorin::match<lit>(*litBegin)) {
        if (auto cls_ax = match<cls>(*litBegin)) {
            switch (cls_ax.id()) {
                case cls::d: has_cls_d = true; break;
                case cls::D: has_cls_D = true; break;
                case cls::w: has_cls_w = true; break;
                case cls::W: has_cls_W = true; break;
                case cls::s: has_cls_s = true; break;
                case cls::S: has_cls_S = true; break;
                case cls::any: unreachable();
            }
        }
        litBegin++;
    }
    if (litBegin == args.end()) return;

    std::set<const Def*> toRemove;
    std::vector<const Def*> toInsert;
    auto& world = (*litBegin)->world();

    auto get           = [](const Def* lt) { return Lit::as(thorin::match<lit, false>(lt)->arg()); };
    auto matchSequence = [get, args_end = args.end()](auto it, int length) -> bool {
        auto last = get(*it);
        for (auto end = it + length; it != end; ++it) {
            if (it == args_end) return false;
            (*it)->world().DLOG("{}: {}", last, get(*it));
            if (last++ != get(*it)) return false;
        }
        return true;
    };
    auto capitalLettersIt = args.end(), underscoreIt = args.end(), tabLfIt = args.end(), crIt = args.end();
    bool hasDigit = has_cls_d;

    for (auto it = litBegin; it != args.end(); ++it) {
        (*it)->world().DLOG("{}\n", get(*it));
        const auto val = get(*it);
        // d
        if (val >= 48 && val < 58) {
            if (has_cls_d) {
                toRemove.insert(*it);
                continue;
            }
        } else if (has_cls_D) {
            toRemove.insert(*it);
            continue;
        }

        if (val == 48 /* 0 */ && matchSequence(it, 10)) {
            std::copy(it, it + 10, std::inserter(toRemove, toRemove.end()));
            toInsert.push_back(world.annex(cls::d));
            hasDigit = true;
        }

        // w
        if ((val >= 65 && val < 91) || (val >= 97 && val < 123) || val == 95) {
            if (has_cls_w) {
                toRemove.insert(*it);
                continue;
            }
        } else if (has_cls_W) {
            toRemove.insert(*it);
            continue;
        }

        if (val == 65 /* A */ && matchSequence(it, 26)) capitalLettersIt = it;
        if (val == 95 /* _ */) underscoreIt = it;
        if (hasDigit && capitalLettersIt != args.end() && underscoreIt != args.end()
            && val == 97 /* A */ && matchSequence(it, 26)) {
            std::copy(capitalLettersIt, capitalLettersIt + 26, std::inserter(toRemove, toRemove.end()));
            std::copy(it, it + 26, std::inserter(toRemove, toRemove.end()));
            toRemove.insert(*underscoreIt);
            toInsert.push_back(world.annex(cls::w));
        }

        // s
        if (val == 9 || val == 10 || val == 13 || val == 32) {
            if (has_cls_s) {
                toRemove.insert(*it);
                continue;
            }
        } else if (has_cls_S) {
            toRemove.insert(*it);
            continue;
        }

        if (val == 9 && get(*(it + 1)) == 10) tabLfIt = it;
        if (val == 13) crIt = it;
        if (val == 32 && tabLfIt != args.end() && crIt != args.end()) {
            toRemove.insert(*tabLfIt);
            toRemove.insert(*(tabLfIt + 1));
            toRemove.insert(*crIt);
            toRemove.insert(*it);
            toInsert.push_back(world.annex(cls::s));
        }
    }
    std::erase_if(args, [&toRemove](const Def* val) -> bool { return toRemove.contains(val); });
    std::copy(toInsert.begin(), toInsert.end(), std::back_inserter(args));

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
        reduceLitsToClass(newArgs);

        const Def* toRemove = nullptr;
        for (const auto* cls0 : newArgs)
            for (const auto* cls1 : newArgs)
                if (equals_any<cls::d, cls::D>(cls0, cls1) || equals_any<cls::w, cls::W>(cls0, cls1)
                    || equals_any<cls::s, cls::S>(cls0, cls1))
                    // A || !A == True
                    return world.annex(cls::any);
                else if (equals_any<cls::w, cls::d>(cls0, cls1))
                    // d is a subset of w
                    toRemove = world.annex(cls::d);
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
    auto& world = type->world();
    return world.raw_app(type, callee, arg);
}

Ref normalize_lit(Ref type, Ref callee, Ref arg) {
    auto& world = type->world();
    return world.raw_app(type, callee, arg);
}

THORIN_regex_NORMALIZER_IMPL

} // namespace thorin::regex
