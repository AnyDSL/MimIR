#include <rang.hpp>

#include <mim/tuple.h>
#include <mim/world.h>

#include "mim/plug/refly/refly.h"
#include "mim/plug/refly/pass/remove_perm.h"

using namespace std::string_literals;

namespace mim::plug::refly {

static_assert(sizeof(void*) <= sizeof(u64), "pointer doesn't fit into Lit");

namespace {

// The trick is that we simply "box" the pointer of @p def inside a Lit of type `%refly.Code`.
const Def* do_reify(const Def* def) {
    auto& world = def->world();
    return world.lit(world.call<Code>(def->type()), reinterpret_cast<u64>(def));
}

// And here we are doing the reverse to retrieve the original pointer again.
const Def* do_reflect(const Def* def) { return reinterpret_cast<const Def*>(def->as<Lit>()->get()); }

void debug_print(const Def* lvl, const Def* def) {
    auto& world = def->world();
    auto level  = Log::Level::Debug;
    if (auto l = Lit::isa(lvl))
        level = (nat_t)Log::Level::Error <= *l && *l <= (nat_t)Log::Level::Debug ? (Log::Level)*l : Log::Level::Debug;
    world.log().log(level, __FILE__, __LINE__, "{}debug_print: {}{}", rang::fg::yellow, def, rang::fg::reset);
    world.log().log(level, def->loc(), "def : {}", def);
    world.log().log(level, def->loc(), "id  : {}", def->unique_name());
    world.log().log(level, def->type()->loc(), "type: {}", def->type());
    world.log().log(level, def->loc(), "node: {}", def->node_name());
    world.log().log(level, def->loc(), "ops : {}", def->num_ops());
    world.log().log(level, def->loc(), "proj: {}", def->num_projs());
    world.log().log(level, def->loc(), "eops: {}", def->num_deps());
}

} // namespace

template<dbg id>
const Def* normalize_dbg(const Def*, const Def*, const Def* arg) {
    auto [lvl, x] = arg->projs<2>();
    debug_print(lvl, x);
    return id == dbg::perm ? nullptr : x;
}

const Def* normalize_reify(const Def*, const Def*, const Def* arg) { return do_reify(arg); }

const Def* normalize_reflect(const Def*, const Def*, const Def* arg) { return do_reflect(arg); }

const Def* normalize_refine(const Def*, const Def*, const Def* arg) {
    auto [code, i, x] = arg->projs<3>();
    if (auto l = Lit::isa(i)) {
        auto def = do_reflect(code);
        return do_reify(def->refine(*l, do_reflect(x)));
    }

    return {};
}

const Def* normalize_type(const Def*, const Def*, const Def* arg) { return arg->type(); }
const Def* normalize_gid(const Def*, const Def*, const Def* arg) { return arg->world().lit_nat(arg->gid()); }

template<equiv id>
const Def* normalize_equiv(const Def*, const Def*, const Def* arg) {
    auto [a, b] = arg->projs<2>();
    bool eq     = id & (equiv::aE & 0xff);

    if (id & (equiv::Ae & 0xff)) {
        auto res = Checker::alpha<Checker::Test>(a, b);
        if (res ^ eq) mim::error(arg->loc(), "'{}' and '{}' {}alpha-equivalent", a, b, !res ? "not " : "");
    } else {
        auto res = a == b;
        if (res ^ eq) mim::error(arg->loc(), "'{}' and '{}' {}structural-equivalent", a, b, !res ? "not " : "");
    }
    return a;
}

const Def* normalize_check(const Def* type, const Def*, const Def* arg) {
    auto& w               = type->world();
    auto [cond, val, msg] = arg->projs<3>();

    if (cond == w.lit_tt()) return val;
    if (cond == w.lit_ff()) {
        auto s = tuple2str(msg);
        if (s.empty()) s = "unknown error"s;
        w.ELOG(s.c_str());
    }

    return nullptr;
}

template<pass>
const Def* normalize_pass(const Def* t, const Def*, const Def*) { return create<RemoveDbgPerm>(pass::remove_dbg_perm, t); }

MIM_refly_NORMALIZER_IMPL

} // namespace mim::plug::refly
