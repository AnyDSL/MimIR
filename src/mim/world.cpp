#include "mim/world.h"

#include <string>

#include "mim/check.h"
#include "mim/def.h"
#include "mim/driver.h"
#include "mim/rewrite.h"
#include "mim/tuple.h"

#include "mim/util/util.h"

namespace mim {

namespace {
bool is_shape(const Def* s) {
    if (s->isa<Nat>()) return true;
    if (auto arr = s->isa<Arr>()) return arr->body()->isa<Nat>();
    if (auto sig = s->isa_imm<Sigma>())
        return std::ranges::all_of(sig->ops(), [](const Def* op) { return op->isa<Nat>(); });

    return false;
}

} // namespace

/*
 * constructor & destructor
 */

#if (!defined(_MSC_VER) && defined(NDEBUG))
bool World::Lock::guard_ = false;
#endif

World::World(Driver* driver, const State& state)
    : driver_(driver)
    , state_(state) {
    data_.univ        = insert<Univ>(0, *this);
    data_.lit_univ_0  = lit_univ(0);
    data_.lit_univ_1  = lit_univ(1);
    data_.type_0      = type(lit_univ_0());
    data_.type_1      = type(lit_univ_1());
    data_.type_bot    = insert<Bot>(0, type());
    data_.type_top    = insert<Top>(0, type());
    data_.sigma       = insert<Sigma>(0, type(), Defs{})->as<Sigma>();
    data_.tuple       = insert<Tuple>(0, sigma(), Defs{})->as<Tuple>();
    data_.type_nat    = insert<mim::Nat>(0, *this);
    data_.type_idx    = insert<mim::Idx>(0, pi(type_nat(), type()));
    data_.top_nat     = insert<Top>(0, type_nat());
    data_.lit_nat_0   = lit_nat(0);
    data_.lit_nat_1   = lit_nat(1);
    data_.lit_0_1     = lit_idx(1, 0);
    data_.type_bool   = type_idx(2);
    data_.lit_bool[0] = lit_idx(2, 0_u64);
    data_.lit_bool[1] = lit_idx(2, 1_u64);
    data_.lit_nat_max = lit_nat(nat_t(-1));
}

World::World(Driver* driver)
    : World(driver, State()) {}

World::~World() {
    for (auto def : move_.defs) def->~Def();
}

/*
 * Driver
 */

Log& World::log() { return driver().log(); }
Flags& World::flags() { return driver().flags(); }

Sym World::sym(const char* s) { return driver().sym(s); }
Sym World::sym(std::string_view s) { return driver().sym(s); }
Sym World::sym(const std::string& s) { return driver().sym(s); }

const Def* World::register_annex(flags_t f, const Def* def) {
    DLOG("register: 0x{x} -> {}", f, def);
    auto plugin = Annex::demangle(driver(), f);
    if (driver().is_loaded(plugin)) {
        assert_emplace(move_.flags2annex, f, def);
        return def;
    }
    return nullptr;
}

void World::make_external(Def* def) {
    assert(!def->is_external());
    assert(def->is_closed());
    def->external_ = true;
    assert_emplace(move_.sym2external, def->sym(), def);
}

void World::make_internal(Def* def) {
    assert(def->is_external());
    def->external_ = false;
    auto num       = move_.sym2external.erase(def->sym());
    assert_unused(num == 1);
}

/*
 * factory methods
 */

const Type* World::type(const Def* level) {
    if (!level->type()->isa<Univ>())
        error(level->loc(), "argument `{}` to `Type` must be of type `Univ` but is of type `{}`", level, level->type());

    return unify<Type>(1, level)->as<Type>();
}

const Def* World::uinc(const Def* op, level_t offset) {
    if (!op->type()->isa<Univ>())
        error(op->loc(), "operand '{}' of a universe increment must be of type `Univ` but is of type `{}`", op,
              op->type());

    if (auto l = Lit::isa(op)) return lit_univ(*l + 1);
    return unify<UInc>(1, op, offset);
}

template<Sort sort> const Def* World::umax(Defs ops_) {
    // consume nested umax
    DefVec ops;
    for (auto op : ops_)
        if (auto um = op->isa<UMax>())
            ops.insert(ops.end(), um->ops().begin(), um->ops().end());
        else
            ops.emplace_back(op);

    level_t lvl = 0;
    for (auto& op : ops) {
        const Def* r = op;
        if (sort == Sort::Term) r = r->unfold_type();
        if (sort <= Sort::Type) r = r->unfold_type();
        if (sort <= Sort::Kind) {
            if (auto type = r->isa<Type>())
                r = type->level();
            else
                error(r->loc(), "operand '{}' must be a Type of some level", r);
        }

        if (!r->type()->isa<Univ>())
            error(r->loc(), "operand '{}' of a universe max must be of type 'Univ' but is of type '{}'", r, r->type());

        op = r;

        if (auto l = Lit::isa(op))
            lvl = std::max(lvl, *l);
        else
            lvl = level_t(-1);
    }

    const Def* ldef;
    if (lvl != level_t(-1))
        ldef = lit_univ(lvl);
    else {
        std::ranges::sort(ops, [](auto op1, auto op2) { return op1->gid() < op2->gid(); });
        ops.erase(std::unique(ops.begin(), ops.end()), ops.end());
        ldef = unify<UMax>(ops.size(), *this, ops);
    }

    return sort == Sort::Univ ? ldef : type(ldef);
}

// TODO more thorough & consistent checks for singleton types

const Def* World::var(const Def* type, Def* mut) {
    if (auto s = Idx::isa(type)) {
        if (auto l = Lit::isa(s); l && l == 1) return lit_0_1();
    }

    if (auto var = mut->var_) return var;
    return mut->var_ = unify<Var>(1, type, mut);
}

template<bool Normalize> const Def* World::implicit_app(const Def* callee, const Def* arg) {
    while (auto pi = Pi::isa_implicit(callee->type())) callee = app(callee, mut_hole(pi->dom()));
    return app<Normalize>(callee, arg);
}

template<bool Normalize> const Def* World::app(const Def* callee, const Def* arg) {
    callee = callee->zonk();
    arg    = arg->zonk();
    if (auto pi = callee->type()->isa<Pi>()) {
        if (auto new_arg = Checker::assignable(pi->dom(), arg)) {
            arg = new_arg->zonk();
            if (auto imm = callee->isa_imm<Lam>()) return imm->body();

            if (auto lam = callee->isa_mut<Lam>(); lam && lam->is_set() && lam->filter() != lit_ff()) {
                if (auto var = lam->has_var()) {
                    if (auto i = move_.substs.find({var, arg}); i != move_.substs.end()) {
                        // Is there a cached version?
                        auto [filter, body] = i->second->defs<2>();
                        if (filter == lit_tt()) return body;
                    } else {
                        // First check filter, If true, reduce body and cache reduct.
                        auto rw     = VarRewriter(var, arg);
                        auto filter = rw.rewrite(lam->filter());
                        if (filter == lit_tt()) {
                            DLOG("partial evaluate: {} ({})", lam, arg);
                            auto body        = rw.rewrite(lam->body());
                            auto num_bytes   = sizeof(Reduct) + 2 * sizeof(const Def*);
                            auto buf         = move_.arena.substs.allocate(num_bytes, alignof(const Def*));
                            auto reduct      = new (buf) Reduct(2);
                            reduct->defs_[0] = filter;
                            reduct->defs_[1] = body;
                            assert_emplace(move_.substs, std::pair{var, arg}, reduct);
                            return body;
                        }
                    }
                } else if (lam->filter() == lit_tt()) {
                    return lam->body();
                }
            }

            auto type                 = pi->reduce(arg)->zonk();
            callee                    = callee->zonk();
            auto [axiom, curry, trip] = Axiom::get(callee);
            if (axiom) {
                curry = curry == 0 ? trip : curry;
                curry = curry == Axiom::Trip_End ? curry : curry - 1;

                if (auto normalizer = axiom->normalizer(); Normalize && normalizer && curry == 0) {
                    if (auto norm = normalizer(type, callee, arg)) return norm;
                }
            }

            return raw_app(axiom, curry, trip, type, callee, arg);
        }

        throw Error()
            .error(arg->loc(), "cannot apply argument to callee")
            .note(callee->loc(), "callee: '{}'", callee)
            .note(arg->loc(), "argument: '{}'", arg)
            .note(callee->loc(), "vvv domain type vvv\n'{}'\n'{}'", pi->dom(), arg->type())
            .note(arg->loc(), "^^^ argument type ^^^");
    }

    throw Error()
        .error(callee->loc(), "called expression not of function type")
        .error(callee->loc(), "'{}' <--- callee type", callee->type());
}

const Def* World::raw_app(const Def* type, const Def* callee, const Def* arg) {
    auto [axiom, curry, trip] = Axiom::get(callee);
    if (axiom) {
        curry = curry == 0 ? trip : curry;
        curry = curry == Axiom::Trip_End ? curry : curry - 1;
    }

    return raw_app(axiom, curry, trip, type, callee, arg);
}

const Def* World::raw_app(const Axiom* axiom, u8 curry, u8 trip, const Def* type, const Def* callee, const Def* arg) {
    return unify<App>(2, axiom, curry, trip, type, callee, arg);
}

const Def* World::sigma(Defs ops) {
    auto n = ops.size();
    if (n == 0) return sigma();
    if (n == 1) return ops[0];
    if (auto uni = Checker::is_uniform(ops)) return arr(n, uni);
    return unify<Sigma>(ops.size(), Sigma::infer(*this, ops), ops);
}

const Def* World::tuple(Defs ops) {
    auto n = ops.size();
    if (n == 0) return tuple();
    if (n == 1) return ops[0];

    auto sigma = Tuple::infer(*this, ops);
    auto t     = tuple(sigma, ops);
    auto new_t = Checker::assignable(sigma, t);
    if (!new_t)
        error(t->loc(), "cannot assign tuple '{}' of type '{}' to incompatible tuple type '{}'", t, t->type(), sigma);

    return new_t;
}

const Def* World::tuple(const Def* type, Defs ops) {
    // TODO type-check type vs inferred type

    auto n = ops.size();
    if (!type->isa_mut<Sigma>()) {
        if (n == 0) return tuple();
        if (n == 1) return ops[0];
        if (auto uni = Checker::is_uniform(ops)) return pack(n, uni);
    }

    if (n != 0) {
        // eta rule for tuples:
        // (extract(tup, 0), extract(tup, 1), extract(tup, 2)) -> tup
        if (auto extract = ops[0]->isa<Extract>()) {
            auto tup = extract->tuple();
            bool eta = tup->type() == type;
            for (size_t i = 0; i != n && eta; ++i) {
                if (auto extract = ops[i]->isa<Extract>()) {
                    if (auto index = Lit::isa(extract->index())) {
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

    return unify<Tuple>(ops.size(), type, ops);
}

const Def* World::tuple(Sym sym) {
    DefVec defs;
    std::ranges::transform(sym, std::back_inserter(defs), [this](auto c) { return lit_i8(c); });
    return tuple(defs);
}

const Def* World::extract(const Def* d, const Def* index) {
    if (index->isa<Tuple>()) {
        auto n   = index->num_ops();
        auto idx = DefVec(n, [&](size_t i) { return index->op(i); });
        auto ops = DefVec(n, [&](size_t i) { return d->proj(n, Lit::as(idx[i])); });
        return tuple(ops);
    } else if (index->isa<Pack>()) {
        auto ops = DefVec(index->as_lit_arity(), [&](size_t) { return extract(d, index->ops().back()); });
        return tuple(ops);
    }

    const Def* size = Idx::isa(index->type());
    const Def* type = d->unfold_type()->zonk();

    if (auto l = Lit::isa(size); l && *l == 1) {
        if (auto l = Lit::isa(index); !l || *l != 0) WLOG("unknown Idx of size 1: {}", index);
        if (auto sigma = type->isa_mut<Sigma>(); sigma && sigma->num_ops() == 1) {
            // mut sigmas can be 1-tuples; TODO mutables Arr?
        } else {
            return d;
        }
    }

    if (auto pack = d->isa_imm<Pack>()) return pack->body();

    if (!Checker::alpha<Checker::Check>(type->arity(), size))
        error(index->loc(), "index '{}' does not fit within arity '{}'", index, type->arity());

    // extract(insert(x, index, val), index) -> val
    if (auto insert = d->isa<Insert>()) {
        if (index == insert->index()) return insert->value();
    }

    if (auto i = Lit::isa(index)) {
        if (auto hole = d->isa_mut<Hole>()) d = hole->tuplefy();
        if (auto tuple = d->isa<Tuple>()) return tuple->op(*i);

        // extract(insert(x, j, val), i) -> extract(x, i) where i != j (guaranteed by rule above)
        if (auto insert = d->isa<Insert>()) {
            if (insert->index()->isa<Lit>()) return extract(insert->tuple(), index);
        }

        if (auto sigma = type->isa<Sigma>()) {
            if (auto var = sigma->has_var()) {
                auto t = VarRewriter(var, d).rewrite(sigma->op(*i));
                return unify<Extract>(2, t, d, index);
            }

            return unify<Extract>(2, sigma->op(*i), d, index);
        }
    }

    const Def* elem_t;
    if (auto arr = type->isa<Arr>())
        elem_t = arr->reduce(index);
    else
        elem_t = extract(tuple(type->as<Sigma>()->ops()), index);

    assert(d);
    return unify<Extract>(2, elem_t, d, index);
}

const Def* World::insert(const Def* d, const Def* index, const Def* val) {
    auto type = d->unfold_type();
    auto size = Idx::isa(index->type());
    auto lidx = Lit::isa(index);

    if (!Checker::alpha<Checker::Check>(type->arity(), size))
        error(index->loc(), "index '{}' does not fit within arity '{}'", index, type->arity());

    if (lidx) {
        auto elem_type = type->proj(*lidx);
        auto new_val   = Checker::assignable(elem_type, val);
        if (!new_val) {
            throw Error()
                .error(val->loc(), "value to be inserted not assignable to element")
                .note(val->loc(), "vvv value type vvv \n'{}'\n'{}'", val->type(), elem_type)
                .note(val->loc(), "^^^ element type ^^^", elem_type);
        }
        val = new_val;
    }

    if (auto l = Lit::isa(size); l && *l == 1)
        return tuple(d, {val}); // d could be mut - that's why the tuple ctor is needed

    // insert((a, b, c, d), 2, x) -> (a, b, x, d)
    if (auto t = d->isa<Tuple>(); t && lidx) return t->refine(*lidx, val);

    // insert(‹4; x›, 2, y) -> (x, x, y, x)
    if (auto pack = d->isa<Pack>(); pack && lidx) {
        if (auto a = pack->isa_lit_arity(); a && *a < flags().scalarize_threshold) {
            auto new_ops   = DefVec(*a, pack->body());
            new_ops[*lidx] = val;
            return tuple(type, new_ops);
        }
    }

    // insert(insert(x, index, y), index, val) -> insert(x, index, val)
    if (auto insert = d->isa<Insert>()) {
        if (insert->index() == index) d = insert->tuple();
    }

    return unify<Insert>(3, d, index, val);
}

// TODO merge this code with pack
const Def* World::arr(const Def* shape, const Def* body) {
    if (!is_shape(shape->type())) error(shape->loc(), "expected shape but got '{}' of type '{}'", shape, shape->type());

    if (auto a = Lit::isa(shape)) {
        if (*a == 0) return sigma();
        if (*a == 1) return body;
    }

    // «(a, b)#i; T» -> («a, T», <b, T»)#i
    if (auto ex = shape->isa<Extract>()) {
        if (auto tup = ex->tuple()->isa<Tuple>()) {
            auto arrs = DefVec(tup->num_ops(), [&](size_t i) { return arr(tup->op(i), body); });
            return extract(tuple(arrs), ex->index());
        }
    }

    // «(a, b, c); body» -> «a; «(b, c); body»»
    if (auto tuple = shape->isa<Tuple>()) return arr(tuple->ops().front(), arr(tuple->ops().subspan(1), body));

    // «<n; x>; body» -> «x; «<n-1, x>; body»»
    if (auto p = shape->isa<Pack>()) {
        if (auto s = Lit::isa(p->shape())) return arr(*s, arr(pack(*s - 1, p->body()), body));
    }

    return unify<Arr>(2, body->unfold_type(), shape, body);
}

const Def* World::pack(const Def* shape, const Def* body) {
    if (!is_shape(shape->type())) error(shape->loc(), "expected shape but got '{}' of type '{}'", shape, shape->type());

    if (auto a = Lit::isa(shape)) {
        if (*a == 0) return tuple();
        if (*a == 1) return body;
    }

    // <(a, b, c); body> -> <a; «(b, c); body>>
    if (auto tuple = shape->isa<Tuple>()) return pack(tuple->ops().front(), pack(tuple->ops().subspan(1), body));

    // <<n; x>; body> -> <x; <<n-1, x>; body>>
    if (auto p = shape->isa<Pack>()) {
        if (auto s = Lit::isa(p->shape())) return pack(*s, pack(pack(*s - 1, p->body()), body));
    }

    auto type = arr(shape, body->type());
    return unify<Pack>(1, type, body);
}

const Def* World::arr(Defs shape, const Def* body) {
    if (shape.empty()) return body;
    return arr(shape.rsubspan(1), arr(shape.back(), body));
}

const Def* World::pack(Defs shape, const Def* body) {
    if (shape.empty()) return body;
    return pack(shape.rsubspan(1), pack(shape.back(), body));
}

const Lit* World::lit(const Def* type, u64 val) {
    if (auto size = Idx::isa(type)) {
        if (auto s = Lit::isa(size)) {
            if (*s != 0 && val >= *s) error(type->loc(), "index '{}' does not fit within arity '{}'", size, val);
        } else if (val != 0) { // 0 of any size is allowed
            error(type->loc(), "cannot create literal '{}' of 'Idx {}' as size is unknown", val, size);
        }
    }

    return unify<Lit>(0, type, val);
}

/*
 * set
 */

template<bool Up> const Def* World::ext(const Def* type) {
    if (auto arr = type->isa<Arr>()) return pack(arr->shape(), ext<Up>(arr->body()));
    if (auto sigma = type->isa<Sigma>())
        return tuple(sigma, DefVec(sigma->num_ops(), [&](size_t i) { return ext<Up>(sigma->op(i)); }));
    return unify<TExt<Up>>(0, type);
}

template<bool Up> const Def* World::bound(Defs ops) {
    auto kind = umax<Sort::Type>(ops);

    // has ext<Up> value?
    if (std::ranges::any_of(ops, [&](const Def* op) { return Up ? bool(op->isa<Top>()) : bool(op->isa<Bot>()); }))
        return ext<Up>(kind);

    // ignore: ext<!Up>
    DefVec cpy(ops.begin(), ops.end());
    auto [_, end] = std::ranges::copy_if(ops, cpy.begin(), [&](const Def* op) { return !op->isa<Ext>(); });

    // sort and remove duplicates
    std::sort(cpy.begin(), end, GIDLt<const Def*>());
    end = std::unique(cpy.begin(), end);
    cpy.resize(std::distance(cpy.begin(), end));

    if (cpy.size() == 0) return ext<!Up>(kind);
    if (cpy.size() == 1) return cpy[0];

    // TODO simplify mixed terms with joins and meets

    return unify<TBound<Up>>(cpy.size(), kind, cpy);
}

const Def* World::merge(const Def* type, Defs ops) {
    if (type->isa<Meet>()) {
        auto types = DefVec(ops.size(), [&](size_t i) { return ops[i]->type(); });
        return unify<Merge>(ops.size(), meet(types), ops);
    }

    assert(ops.size() == 1);
    return ops[0];
}

const Def* World::merge(Defs ops) { return merge(umax<Sort::Term>(ops), ops); }

const Def* World::inj(const Def* type, const Def* value) {
    if (type->isa<Join>()) return unify<Inj>(1, type, value);
    return value;
}

const Def* World::split(const Def* type, const Def* value) { return unify<Split>(1, type, value); }

const Def* World::test(Defs ops_) {
    if (ops_.size() == 1) return ops_.front();

    auto ops   = DefVec(ops_.begin(), ops_.end());
    auto value = ops.front();
    auto arms  = ops.span().subspan(1);
    auto join  = value->type()->isa<Join>();

    if (!join) error(value->loc(), "scrutinee of a test expression must be of union type");

    if (arms.size() != join->num_ops())
        error(value->loc(), "test expression has {} arms but union type has {} cases", arms.size(), join->num_ops());

    for (auto arm : arms)
        if (!arm->type()->isa<Pi>())
            error(arm->loc(), "arm of test expression does not have a function type but is of type '{}'", arm->type());

    std::ranges::sort(arms, [](const Def* arm1, const Def* arm2) {
        return arm1->type()->as<Pi>()->dom()->gid() < arm2->type()->as<Pi>()->dom()->gid();
    });

    const Def* type = nullptr;
    for (size_t i = 0, e = arms.size(); i != e; ++i) {
        auto arm = arms[i];
        auto pi  = arm->type()->as<Pi>();
        if (!Checker::alpha<Checker::Check>(pi->dom(), join->op(i)))
            error(arm->loc(),
                  "domain type '{}' of arm in a test expression does not match case type '{}' in union type", pi->dom(),
                  join->op(i));
        type = type ? this->join({type, pi->codom()}) : pi->codom();
    }

    return unify<Test>(ops.size(), type, ops);
}

const Def* World::uniq(const Def* inhabitant) { return unify<Uniq>(1, inhabitant->type()->unfold_type(), inhabitant); }

Sym World::append_suffix(Sym symbol, std::string suffix) {
    auto name = symbol.str();

    auto pos = name.find(suffix);
    if (pos != std::string::npos) {
        auto num = name.substr(pos + suffix.size());
        if (num.empty()) {
            name += "_1";
        } else {
            num  = num.substr(1);
            num  = std::to_string(std::stoi(num) + 1);
            name = name.substr(0, pos + suffix.size()) + "_" + num;
        }
    } else {
        name += suffix;
    }

    return sym(std::move(name));
}

Defs World::reduce(const Var* var, const Def* arg) {
    auto mut    = var->mut();
    auto offset = mut->reduction_offset();
    auto size   = mut->num_ops() - offset;

    if (auto i = move_.substs.find({var, arg}); i != move_.substs.end()) return i->second->defs();

    auto buf    = move_.arena.substs.allocate(sizeof(Reduct) + size * sizeof(const Def*), alignof(const Def*));
    auto reduct = new (buf) Reduct(size);
    auto rw     = VarRewriter(var, arg);
    for (size_t i = 0; i != size; ++i) reduct->defs_[i] = rw.rewrite(mut->op(i + offset));
    assert_emplace(move_.substs, std::pair{var, arg}, reduct);
    return reduct->defs();
}

/*
 * debugging
 */

#ifdef MIM_ENABLE_CHECKS

void World::breakpoint(u32 gid) { state_.breakpoints.emplace(gid); }

const Def* World::gid2def(u32 gid) {
    auto i = std::ranges::find_if(move_.defs, [=](auto def) { return def->gid() == gid; });
    if (i == move_.defs.end()) return nullptr;
    return *i;
}

World& World::verify() {
    for (auto mut : externals()) assert(mut->is_closed() && mut->is_set());
    for (auto anx : annexes()) assert(anx->is_closed());
    return *this;
}

#endif

#ifndef DOXYGEN
template const Def* World::umax<Sort::Term>(Defs);
template const Def* World::umax<Sort::Type>(Defs);
template const Def* World::umax<Sort::Kind>(Defs);
template const Def* World::umax<Sort::Univ>(Defs);
template const Def* World::ext<true>(const Def*);
template const Def* World::ext<false>(const Def*);
template const Def* World::bound<true>(Defs);
template const Def* World::bound<false>(Defs);
template const Def* World::app<true>(const Def*, const Def*);
template const Def* World::app<false>(const Def*, const Def*);
template const Def* World::implicit_app<true>(const Def*, const Def*);
template const Def* World::implicit_app<false>(const Def*, const Def*);
#endif

} // namespace mim
