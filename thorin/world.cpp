#include "thorin/world.h"

#include "thorin/tuple.h"

// for colored output
#ifdef _WIN32
#    include <io.h>
#    define isatty _isatty
#    define fileno _fileno
#else
#    include <unistd.h>
#endif

#include "thorin/check.h"
#include "thorin/def.h"
#include "thorin/error.h"
#include "thorin/rewrite.h"

#include "thorin/analyses/scope.h"
#include "thorin/util/array.h"
#include "thorin/util/container.h"

namespace thorin {

/*
 * constructor & destructor
 */

#if (!defined(_MSC_VER) && defined(NDEBUG))
bool World::Arena::Lock::guard_ = false;
#endif

World::Move::Move(World& world)
    : checker(std::make_unique<Checker>(world))
    , err(std::make_unique<ErrorHandler>()) {}

World::World(const State& state)
    : state_(state)
    , move_(*this) {
    data_.univ_        = insert<Univ>(0, *this);
    data_.lit_univ_0_  = lit_univ(0);
    data_.lit_univ_1_  = lit_univ(1);
    data_.type_0_      = type(lit_univ_0());
    data_.type_1_      = type(lit_univ_1());
    data_.type_bot_    = insert<Bot>(0, type(), nullptr);
    data_.sigma_       = insert<Sigma>(0, type(), Defs{}, nullptr)->as<Sigma>();
    data_.tuple_       = insert<Tuple>(0, sigma(), Defs{}, nullptr)->as<Tuple>();
    data_.type_nat_    = insert<Nat>(0, *this);
    data_.type_idx_    = insert<Idx>(0, pi(type_nat(), type()));
    data_.top_nat_     = insert<Top>(0, type_nat(), nullptr);
    data_.lit_nat_0_   = lit_nat(0);
    data_.lit_nat_1_   = lit_nat(1);
    data_.type_bool_   = type_idx(2);
    data_.lit_bool_[0] = lit_idx(2, 0_u64);
    data_.lit_bool_[1] = lit_idx(2, 1_u64);
    data_.lit_nat_max_ = lit_nat(nat_t(-1));
    data_.exit_        = nom_lam(cn(type_bot()), dbg("exit"));
}

World::World(std::string_view name /* = {}*/)
    : World(State(name)) {}

World::~World() {
    for (auto def : move_.defs) def->~Def();
}

const Type* World::type(const Def* level, const Def* dbg) {
    if (err()) {
        if (!level->type()->isa<Univ>()) {
            err()->err(level->loc(), "argument `{}` to `.Type` must be of type `.Univ` but is of type `{}`", level,
                       level->type());
        }
    }
    return unify<Type>(1, level, dbg)->as<Type>();
}

const Def* World::app(const Def* callee, const Def* arg, const Def* dbg) {
    auto pi = callee->type()->isa<Pi>();

    if (err()) {
        if (!pi)
            err()->err(dbg->loc(), "called expression '{}' : '{}' is not of function type", callee, callee->type());
        if (!checker().assignable(pi->dom(), arg, dbg)) err()->ill_typed_app(callee, arg, dbg);
    }

    if (auto lam = callee->isa<Lam>(); lam && lam->is_set() && lam->codom()->sort() > Sort::Type)
        return lam->reduce(arg).back();

    auto type = pi->reduce(arg).back();
    return raw_app<true>(type, callee, arg, dbg);
}

template<bool Normalize>
const Def* World::raw_app(const Def* type, const Def* callee, const Def* arg, const Def* dbg) {
    auto [axiom, curry, trip] = Axiom::get(callee);
    if (axiom) {
        curry = curry == 0 ? trip : curry;
        curry = curry == Axiom::Trip_End ? curry : curry - 1;

        if (auto normalize = axiom->normalizer(); Normalize && normalize && curry == 0)
            return normalize(type, callee, arg, dbg);
    }

    return unify<App>(2, axiom, curry, trip, type, callee, arg, dbg);
}

const Def* World::sigma(Defs ops, const Def* dbg) {
    auto n = ops.size();
    if (n == 0) return sigma();
    if (n == 1) return ops[0];
    if (auto uni = checker().is_uniform(ops, dbg)) return arr(n, uni, dbg);
    return unify<Sigma>(ops.size(), infer_type_level(*this, ops), ops, dbg);
}

static const Def* infer_sigma(World& world, Defs ops) {
    DefArray elems(ops.size());
    for (size_t i = 0, e = ops.size(); i != e; ++i) elems[i] = ops[i]->type();

    return world.sigma(elems);
}

const Def* World::tuple(Defs ops, const Def* dbg) {
    if (ops.size() == 1) return ops[0];

    auto sigma = infer_sigma(*this, ops);
    auto t     = tuple(sigma, ops, dbg);
    if (err() && !checker().assignable(sigma, t, dbg)) { assert(false && "TODO: error msg"); }

    return t;
}

const Def* World::tuple(const Def* type, Defs ops, const Def* dbg) {
    if (err()) {
        // TODO type-check type vs inferred type
    }

    auto n = ops.size();
    if (!type->isa_nom<Sigma>()) {
        if (n == 0) return tuple();
        if (n == 1) return ops[0];
        if (auto uni = checker().is_uniform(ops, dbg)) return pack(n, uni, dbg);
    }

    if (n != 0) {
        // eta rule for tuples:
        // (extract(tup, 0), extract(tup, 1), extract(tup, 2)) -> tup
        if (auto extract = ops[0]->isa<Extract>()) {
            auto tup = extract->tuple();
            bool eta = tup->type() == type;
            for (size_t i = 0; i != n && eta; ++i) {
                if (auto extract = ops[i]->isa<Extract>()) {
                    if (auto index = isa_lit(extract->index())) {
                        if (eta &= u64(i) == *index) {
                            eta &= extract->tuple() == tup;
                            continue;
                        }
                    }
                }
                eta = false;
            }

            if (eta) return tup;
        }
    }

    return unify<Tuple>(ops.size(), type, ops, dbg);
}

const Def* World::tuple_str(std::string_view s, const Def* dbg) {
    DefVec ops;
    for (auto c : s) ops.emplace_back(lit_nat(c));
    return tuple(ops, dbg);
}

const Def* World::extract(const Def* d, const Def* index, const Def* dbg) {
    assert(d);
    if (index->isa<Tuple>()) {
        auto n = index->num_ops();
        DefArray idx(n, [&](size_t i) { return index->op(i); });
        DefArray ops(n, [&](size_t i) { return d->proj(n, as_lit(idx[i])); });
        return tuple(ops, dbg);
    } else if (index->isa<Pack>()) {
        DefArray ops(as_lit(index->arity()), [&](size_t) { return extract(d, index->ops().back()); });
        return tuple(ops, dbg);
    }

    auto size = Idx::size(index->type());
    auto type = d->unfold_type();

    // nom sigmas can be 1-tuples
    if (auto l = isa_lit(size); l && *l == 1 && !d->type()->isa_nom<Sigma>()) return d;
    if (auto pack = d->isa_structural<Pack>()) return pack->body();

    if (err()) {
        if (!checker().equiv(type->arity(), size, dbg)) err()->index_out_of_range(type->arity(), index, dbg);
    }

    // extract(insert(x, index, val), index) -> val
    if (auto insert = d->isa<Insert>()) {
        if (index == insert->index()) return insert->value();
    }

    if (auto i = isa_lit(index)) {
        if (auto tuple = d->isa<Tuple>()) return tuple->op(*i);

        // extract(insert(x, j, val), i) -> extract(x, i) where i != j (guaranteed by rule above)
        if (auto insert = d->isa<Insert>()) {
            if (insert->index()->isa<Lit>()) return extract(insert->tuple(), index, dbg);
        }

        if (auto sigma = type->isa<Sigma>()) {
            if (auto nom_sigma = sigma->isa_nom<Sigma>()) {
                Scope scope(nom_sigma);
                auto t = rewrite(sigma->op(*i), nom_sigma->var(), d, scope);
                return unify<Extract>(2, t, d, index, dbg);
            }

            return unify<Extract>(2, sigma->op(*i), d, index, dbg);
        }
    }

    const Def* elem_t;
    if (auto arr = type->isa<Arr>())
        elem_t = arr->reduce(index);
    else
        elem_t = extract(tuple(type->as<Sigma>()->ops(), dbg), index, dbg);

    assert(d);
    return unify<Extract>(2, elem_t, d, index, dbg);
}

const Def* World::insert(const Def* d, const Def* index, const Def* val, const Def* dbg) {
    auto type = d->unfold_type();
    auto size = Idx::size(index->type());

    if (err()) {
        if (!checker().equiv(type->arity(), size, dbg)) err()->index_out_of_range(type->arity(), index, dbg);

        // The value type does not match the type in the tuple at position index.
        if (auto index_lit = isa_lit(index)) {
            auto target_type = type->proj(*index_lit);
            if (!checker().assignable(target_type, val, dbg)) err()->expected_type(target_type, dbg);
        }
    }

    if (auto l = isa_lit(size); l && *l == 1)
        return tuple(d, {val}, dbg); // d could be nom - that's why the tuple ctor is needed

    // insert((a, b, c, d), 2, x) -> (a, b, x, d)
    if (auto t = d->isa<Tuple>()) return t->refine(as_lit(index), val);

    // insert(‹4; x›, 2, y) -> (x, x, y, x)
    if (auto pack = d->isa<Pack>()) {
        if (auto a = isa_lit(pack->arity())) {
            DefArray new_ops(*a, pack->body());
            new_ops[as_lit(index)] = val;
            return tuple(type, new_ops, dbg);
        }
    }

    // insert(insert(x, index, y), index, val) -> insert(x, index, val)
    if (auto insert = d->isa<Insert>()) {
        if (insert->index() == index) d = insert->tuple();
    }

    return unify<Insert>(3, d, index, val, dbg);
}

bool is_shape(const Def* s) {
    if (s->isa<Nat>()) return true;
    if (auto arr = s->isa<Arr>()) return arr->body()->isa<Nat>();
    if (auto sig = s->isa_structural<Sigma>())
        return std::ranges::all_of(sig->ops(), [](const Def* op) { return op->isa<Nat>(); });

    return false;
}

const Def* World::arr(const Def* shape, const Def* body, const Def* dbg) {
    if (err()) {
        if (!is_shape(shape->type())) err()->expected_shape(shape, dbg);
    }

    if (auto a = isa_lit<u64>(shape)) {
        if (*a == 0) return sigma();
        if (*a == 1) return body;
    }

    // «(a, b)#i; T» -> («a, T», <b, T»)#i
    if (auto ex = shape->isa<Extract>()) {
        if (auto tup = ex->tuple()->isa<Tuple>()) {
            DefArray arrs(tup->num_ops(), [&](size_t i) { return arr(tup->op(i), body); });
            return extract(tuple(arrs), ex->index(), dbg);
        }
    }

    // «(a, b, c); body» -> «a; «(b, c); body»»
    if (auto tuple = shape->isa<Tuple>()) return arr(tuple->ops().front(), arr(tuple->ops().skip_front(), body), dbg);

    // «<n; x>; body» -> «x; «<n-1, x>; body»»
    if (auto p = shape->isa<Pack>()) {
        if (auto s = isa_lit(p->shape())) return arr(*s, arr(pack(*s - 1, p->body()), body), dbg);
    }

    return unify<Arr>(2, body->unfold_type(), shape, body, dbg);
}

const Def* World::pack(const Def* shape, const Def* body, const Def* dbg) {
    if (err()) {
        if (!is_shape(shape->type())) err()->expected_shape(shape, dbg);
    }

    if (auto a = isa_lit<u64>(shape)) {
        if (*a == 0) return tuple();
        if (*a == 1) return body;
    }

    // <(a, b, c); body> -> <a; «(b, c); body>>
    if (auto tuple = shape->isa<Tuple>()) return pack(tuple->ops().front(), pack(tuple->ops().skip_front(), body), dbg);

    // <<n; x>; body> -> <x; <<n-1, x>; body>>
    if (auto p = shape->isa<Pack>()) {
        if (auto s = isa_lit(p->shape())) return pack(*s, pack(pack(*s - 1, p->body()), body), dbg);
    }

    auto type = arr(shape, body->type());
    return unify<Pack>(1, type, body, dbg);
}

const Def* World::arr(Defs shape, const Def* body, const Def* dbg) {
    if (shape.empty()) return body;
    return arr(shape.skip_back(), arr(shape.back(), body, dbg), dbg);
}

const Def* World::pack(Defs shape, const Def* body, const Def* dbg) {
    if (shape.empty()) return body;
    return pack(shape.skip_back(), pack(shape.back(), body, dbg), dbg);
}

const Lit* World::lit(const Def* type, u64 val, const Def* dbg) {
    if (auto size = Idx::size(type)) {
        if (err()) {
            if (auto s = isa_lit(size)) {
                if (*s != 0 && val >= *s) err()->index_out_of_range(size, val, dbg);
            } else if (val != 0) { // 0 of any size is allowed
                err()->err(dbg->loc(), "cannot create literal '{}' of '.Idx {}' as size is unknown", val, size);
            }
        }
    }

    return unify<Lit>(0, type, val, dbg);
}

/*
 * set
 */

template<bool up>
const Def* World::ext(const Def* type, const Def* dbg) {
    if (auto arr = type->isa<Arr>()) return pack(arr->shape(), ext<up>(arr->body()), dbg);
    if (auto sigma = type->isa<Sigma>())
        return tuple(sigma, DefArray(sigma->num_ops(), [&](size_t i) { return ext<up>(sigma->op(i), dbg); }), dbg);
    return unify<TExt<up>>(0, type, dbg);
}

template<bool up>
const Def* World::bound(Defs ops, const Def* dbg) {
    auto kind = infer_type_level(*this, ops);

    // has ext<up> value?
    if (std::ranges::any_of(ops, [&](const Def* op) { return up ? bool(op->isa<Top>()) : bool(op->isa<Bot>()); }))
        return ext<up>(kind);

    // ignore: ext<!up>
    DefArray cpy(ops);
    auto [_, end] = std::ranges::copy_if(ops, cpy.begin(), [&](const Def* op) { return !op->isa<Ext>(); });

    // sort and remove duplicates
    std::sort(cpy.begin(), end, GIDLt<const Def*>());
    end = std::unique(cpy.begin(), end);
    cpy.shrink(std::distance(cpy.begin(), end));

    if (cpy.size() == 0) return ext<!up>(kind, dbg);
    if (cpy.size() == 1) return cpy[0];

    // TODO simplify mixed terms with joins and meets

    return unify<TBound<up>>(cpy.size(), kind, cpy, dbg);
}

const Def* World::ac(const Def* type, Defs ops, const Def* dbg) {
    if (type->isa<Meet>()) {
        DefArray types(ops.size(), [&](size_t i) { return ops[i]->type(); });
        return unify<Ac>(ops.size(), meet(types), ops, dbg);
    }

    assert(ops.size() == 1);
    return ops[0];
}

const Def* World::ac(Defs ops, const Def* dbg /*= {}*/) { return ac(infer_type_level(*this, ops), ops, dbg); }

const Def* World::vel(const Def* type, const Def* value, const Def* dbg) {
    if (type->isa<Join>()) return unify<Vel>(1, type, value, dbg);
    return value;
}

const Def* World::pick(const Def* type, const Def* value, const Def* dbg) { return unify<Pick>(1, type, value, dbg); }

const Def* World::test(const Def* value, const Def* probe, const Def* match, const Def* clash, const Def* dbg) {
    auto m_pi = match->type()->isa<Pi>();
    auto c_pi = clash->type()->isa<Pi>();

    if (err()) {
        // TODO proper error msg
        assert(m_pi && c_pi);
        auto a = isa_lit(m_pi->dom()->arity());
        assert_unused(a && *a == 2);
        assert(checker().equiv(m_pi->dom(2, 0_s), c_pi->dom(), nullptr));
    }

    auto codom = join({m_pi->codom(), c_pi->codom()});
    return unify<Test>(4, pi(c_pi->dom(), codom), value, probe, match, clash, dbg);
}

const Def* World::singleton(const Def* inner_type, const Def* dbg) {
    return unify<Singleton>(1, this->type<1>(), inner_type, dbg);
}

/*
 * debugging
 */

#if THORIN_ENABLE_CHECKS

void World::breakpoint(size_t number) { state_.breakpoints.emplace(number); }

const Def* World::gid2def(u32 gid) {
    auto i = std::ranges::find_if(move_.defs, [=](auto def) { return def->gid() == gid; });
    if (i == move_.defs.end()) return nullptr;
    return *i;
}

#endif

/*
 * instantiate templates
 */

#ifndef DOXYGEN // Doxygen doesn't like this
template const Def* World::raw_app<true>(const Def*, const Def*, const Def*, const Def*);
template const Def* World::raw_app<false>(const Def*, const Def*, const Def*, const Def*);
#endif
template const Def* World::ext<true>(const Def*, const Def*);
template const Def* World::ext<false>(const Def*, const Def*);
template const Def* World::bound<true>(Defs, const Def*);
template const Def* World::bound<false>(Defs, const Def*);

} // namespace thorin
