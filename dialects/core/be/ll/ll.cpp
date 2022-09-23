#include "dialects/core/be/ll/ll.h"

#include <deque>
#include <fstream>
#include <iomanip>
#include <limits>
#include <ranges>

#include "thorin/analyses/cfg.h"
#include "thorin/be/emitter.h"
#include "thorin/util/print.h"
#include "thorin/util/sys.h"

#include "dialects/clos/clos.h"
#include "dialects/core/core.h"
#include "dialects/mem/mem.h"

// Lessons learned:
// * **Always** follow all ops - even if you actually want to ignore one.
//   Otherwise, you might end up with an incorrect schedule.
//   This was the case for an Extract of type Mem.
//   While we want to ignore the value obtained from that, since there is no Mem value in LLVM,
//   we still want to **first** recursively emit code for its operands and **then** ignore the Extract itself.
// * i1 has a different meaning in LLVM then in Thorin:
//      * Thorin: {0,  1} = i1
//      * LLVM:   {0, -1} = i1
//   This is a problem when, e.g., using an index of type i1 as LLVM thinks like this:
//   getelementptr ..., i1 1 == getelementptr .., i1 -1
using namespace std::string_literals;

namespace thorin::ll {

static bool is_const(const Def* def) {
    if (def->isa<Bot>()) return true;
    if (def->isa<Lit>()) return true;
    if (auto pack = def->isa_structural<Pack>()) return is_const(pack->shape()) && is_const(pack->body());

    if (auto tuple = def->isa<Tuple>()) {
        auto ops = tuple->ops();
        return std::ranges::all_of(ops, [](auto def) { return is_const(def); });
    }

    return false;
}

struct BB {
    BB()          = default;
    BB(const BB&) = delete;
    BB(BB&& other) { swap(*this, other); }

    std::deque<std::ostringstream>& head() { return parts[0]; }
    std::deque<std::ostringstream>& body() { return parts[1]; }
    std::deque<std::ostringstream>& tail() { return parts[2]; }

    template<class... Args>
    std::string assign(std::string_view name, const char* s, Args&&... args) {
        print(print(body().emplace_back(), "{} = ", name), s, std::forward<Args&&>(args)...);
        return std::string(name);
    }

    template<class... Args>
    void tail(const char* s, Args&&... args) {
        print(tail().emplace_back(), s, std::forward<Args&&>(args)...);
    }

    friend void swap(BB& a, BB& b) {
        using std::swap;
        swap(a.phis, b.phis);
        swap(a.parts, b.parts);
    }

    DefMap<std::vector<std::pair<std::string, std::string>>> phis;
    std::array<std::deque<std::ostringstream>, 3> parts;
};

class Emitter : public thorin::Emitter<std::string, std::string, BB, Emitter> {
public:
    using Super = thorin::Emitter<std::string, std::string, BB, Emitter>;

    Emitter(World& world, std::ostream& ostream)
        : Super(world, "llvm_emitter", ostream) {}

    bool is_valid(std::string_view s) { return !s.empty(); }
    void start() override;
    void emit_imported(Lam*);
    void emit_epilogue(Lam*);
    std::string emit_bb(BB&, const Def*);
    std::string prepare(const Scope&);
    void prepare(Lam*, std::string_view);
    void finalize(const Scope&);

private:
    std::string id(const Def*, bool force_bb = false) const;
    std::string convert(const Def*);
    std::string convert_ret_pi(const Pi*);

    std::ostringstream type_decls_;
    std::ostringstream vars_decls_;
    std::ostringstream func_decls_;
    std::ostringstream func_impls_;
};

/*
 * convert
 */

std::string Emitter::id(const Def* def, bool force_bb /*= false*/) const {
    if (auto global = def->isa<Global>()) return "@" + global->unique_name();

    if (auto lam = def->isa_nom<Lam>(); lam && !force_bb) {
        if (lam->type()->ret_pi()) {
            if (lam->is_external() || !lam->is_set())
                return "@" + lam->name(); // TODO or use is_internal or sth like that?
            return "@" + lam->unique_name();
        }
    }

    return "%" + def->unique_name();
}

std::string Emitter::convert(const Def* type) {
    if (auto i = types_.find(type); i != types_.end()) return i->second;

    assert(!match<mem::M>(type));
    std::ostringstream s;
    std::string name;

    if (type->isa<Nat>()) {
        return types_[type] = "i64";
    } else if (auto idx = type->isa<Idx>()) {
        auto size = idx->size();
        if (size->isa<Top>()) return types_[type] = "i64";
        if (auto width = size2bitwidth(as_lit(size))) {
            switch (*width) {
                // clang-format off
                case  1: return types_[type] = "i1";
                case  2:
                case  4:
                case  8: return types_[type] = "i8";
                case 16: return types_[type] = "i16";
                case 32: return types_[type] = "i32";
                case 64: return types_[type] = "i64";
                // clang-format on
                default: unreachable();
            }
        } else {
            return types_[type] = "i64";
        }
    } else if (auto real = isa<Tag::Real>(type)) {
        switch (as_lit<nat_t>(real->arg())) {
            case 16: return types_[type] = "half";
            case 32: return types_[type] = "float";
            case 64: return types_[type] = "double";
            default: unreachable();
        }
    } else if (auto ptr = match<mem::Ptr>(type)) {
        auto [pointee, addr_space] = ptr->args<2>();
        // TODO addr_space
        print(s, "{}*", convert(pointee));
    } else if (auto arr = type->isa<Arr>()) {
        auto elem_type = convert(arr->body());
        u64 size       = 0;
        if (auto arity = isa_lit(arr->shape())) size = *arity;
        print(s, "[{} x {}]", size, elem_type);
    } else if (auto pi = type->isa<Pi>()) {
        assert(pi->is_returning() && "should never have to convert type of BB");
        print(s, "{} (", convert_ret_pi(pi->ret_pi()));

        std::string_view sep = "";
        auto doms            = pi->doms();
        for (auto dom : doms.skip_back()) {
            if (match<mem::M>(dom)) continue;
            s << sep << convert(dom);
            sep = ", ";
        }

        s << ")*";
    } else if (auto sigma = type->isa<Sigma>()) {
        if (sigma->isa_nom()) {
            name          = id(sigma);
            types_[sigma] = name;
            print(s, "{} = type", name);
        }
        print(s, "{{");
        std::string_view sep = "";
        for (auto t : sigma->ops()) {
            if (match<mem::M>(t)) continue;
            s << sep << convert(t);
            sep = ", ";
        }
        print(s, "}}");
    } else {
        unreachable();
    }

    if (name.empty()) return types_[type] = s.str();

    assert(!s.str().empty());
    type_decls_ << s.str() << '\n';
    return types_[type] = name;
}

std::string Emitter::convert_ret_pi(const Pi* pi) {
    switch (pi->num_doms()) {
        case 0: return "void";
        case 1:
            if (match<mem::M>(pi->dom())) return "void";
            return convert(pi->dom());
        case 2:
            if (match<mem::M>(pi->dom(0))) return convert(pi->dom(1));
            if (match<mem::M>(pi->dom(1))) return convert(pi->dom(0));
            [[fallthrough]];
        default: return convert(pi->dom());
    }
}

/*
 * emit
 */

void Emitter::start() {
    Super::start();

    ostream() << "declare i8* @malloc(i64)" << '\n'; // HACK
    // SJLJ intrinsics (GLIBC Versions)
    ostream() << "declare i32 @_setjmp(i8*) returns_twice" << '\n';
    ostream() << "declare void @longjmp(i8*, i32) noreturn" << '\n';
    ostream() << "declare i64 @jmpbuf_size()" << '\n';
    ostream() << type_decls_.str() << '\n';
    ostream() << func_decls_.str() << '\n';
    ostream() << vars_decls_.str() << '\n';
    ostream() << func_impls_.str() << '\n';
}

void Emitter::emit_imported(Lam* lam) {
    print(func_decls_, "declare {} {}(", convert_ret_pi(lam->type()->ret_pi()), id(lam));

    auto sep  = "";
    auto doms = lam->doms();
    for (auto dom : doms.skip_back()) {
        if (match<mem::M>(dom)) continue;
        print(func_decls_, "{}{}", sep, convert(dom));
        sep = ", ";
    }

    print(func_decls_, ")\n");
}

std::string Emitter::prepare(const Scope& scope) {
    auto lam = scope.entry()->as_nom<Lam>();

    print(func_impls_, "define {} {}(", convert_ret_pi(lam->type()->ret_pi()), id(lam));

    auto sep  = "";
    auto vars = lam->vars();
    for (auto var : vars.skip_back()) {
        if (match<mem::M>(var->type())) continue;
        auto name    = id(var);
        locals_[var] = name;
        print(func_impls_, "{}{} {}", sep, convert(var->type()), name);
        sep = ", ";
    }

    print(func_impls_, ") {{\n");
    return lam->unique_name();
}

void Emitter::finalize(const Scope& scope) {
    for (auto& [lam, bb] : lam2bb_) {
        for (const auto& [phi, args] : bb.phis) {
            print(bb.head().emplace_back(), "{} = phi {} ", id(phi), convert(phi->type()));
            auto sep = "";
            for (const auto& [arg, pred] : args) {
                print(bb.head().back(), "{}[ {}, {} ]", sep, arg, pred);
                sep = ", ";
            }
        }
    }

    for (auto nom : schedule(scope)) {
        if (auto lam = nom->isa_nom<Lam>()) {
            if (lam == scope.exit()) continue;
            assert(lam2bb_.contains(lam));
            auto& bb = lam2bb_[lam];
            print(func_impls_, "{}:\n", lam->unique_name());

            ++tab;
            for (const auto& part : bb.parts) {
                for (const auto& line : part) tab.print(func_impls_, "{}\n", line.str());
            }
            --tab;
            func_impls_ << std::endl;
        }
    }

    print(func_impls_, "}}\n\n");
}

void Emitter::emit_epilogue(Lam* lam) {
    auto app = lam->body()->as<App>();
    auto& bb = lam2bb_[lam];

    if (app->callee() == entry_->ret_var()) { // return
        std::vector<std::string> values;
        std::vector<const Def*> types;

        for (auto arg : app->args()) {
            if (auto val = emit_unsafe(arg); !val.empty()) {
                values.emplace_back(val);
                types.emplace_back(arg->type());
            }
        }

        switch (values.size()) {
            case 0: return bb.tail("ret void");
            case 1: return bb.tail("ret {} {}", convert(types[0]), values[0]);
            default: {
                std::string prev = "undef";
                auto type        = convert(world().sigma(types));
                for (size_t i = 0, n = values.size(); i != n; ++i) {
                    auto elem   = values[i];
                    auto elem_t = convert(types[i]);
                    auto namei  = "%ret_val." + std::to_string(i);
                    bb.tail("{} = insertvalue {} {}, {} {}, {}", namei, type, prev, elem_t, elem, i);
                    prev = namei;
                }

                bb.tail("ret {} {}", type, prev);
            }
        }
    } else if (auto ex = app->callee()->isa<Extract>(); ex && app->callee_type()->is_basicblock()) {
        emit_unsafe(app->arg());
        auto c = emit(ex->index());
        if (ex->tuple()->num_projs() == 2) {
            auto [f, t] = ex->tuple()->projs<2>([this](auto def) { return emit(def); });
            return bb.tail("br i1 {}, label {}, label {}", c, t, f);
        } else {
            auto c_ty = convert(ex->index()->type());
            bb.tail("switch {} {}, label {} [ ", c_ty, c, emit(ex->tuple()->proj(0)));
            for (auto i = 1u; i < ex->tuple()->num_projs(); i++)
                print(bb.tail().back(), "{} {}, label {} ", c_ty, std::to_string(i), emit(ex->tuple()->proj(i)));
            print(bb.tail().back(), "]");
        }
    } else if (app->callee()->isa<Bot>()) {
        return bb.tail("ret ; bottom: unreachable");
    } else if (auto callee = app->callee()->isa_nom<Lam>(); callee && callee->is_basicblock()) { // ordinary jump
        for (size_t i = 0, e = callee->num_vars(); i != e; ++i) {
            if (auto arg = emit_unsafe(app->arg(i)); !arg.empty()) {
                auto phi = callee->var(i);
                assert(!match<mem::M>(phi->type()));
                lam2bb_[callee].phis[phi].emplace_back(arg, id(lam, true));
                locals_[phi] = id(phi);
            }
        }
        return bb.tail("br label {}", id(callee));
    } else if (auto longjmp = match<clos::longjmp>(app)) {
        auto [mem, jbuf, tag] = app->args<3>();
        emit_unsafe(mem);
        auto emitted_jb  = emit(jbuf);
        auto emitted_tag = emit(tag);
        bb.tail("call void @longjmp(i8* {}, i32 {})", emitted_jb, emitted_tag);
        return bb.tail("unreachable");
    } else if (app->callee_type()->is_returning()) { // function call
        auto emmited_callee = emit(app->callee());

        std::vector<std::string> args;
        auto app_args = app->args();
        for (auto arg : app_args.skip_back()) {
            if (auto emitted_arg = emit_unsafe(arg); !emitted_arg.empty())
                args.emplace_back(convert(arg->type()) + " " + emitted_arg);
        }

        if (app->args().back()->isa<Bot>()) {
            // TODO: Perhaps it'd be better to simply η-wrap this prior to the BE...
            assert(convert_ret_pi(app->callee_type()->ret_pi()) == "void");
            bb.tail("call void {}({, })", emmited_callee, args);
            return bb.tail("unreachable");
        }

        auto ret_lam    = app->args().back()->as_nom<Lam>();
        size_t num_vars = ret_lam->num_vars();
        size_t n        = 0;
        Array<const Def*> values(num_vars);
        Array<const Def*> types(num_vars);
        for (auto var : ret_lam->vars()) {
            if (match<mem::M>(var->type())) continue;
            values[n] = var;
            types[n]  = var->type();
            ++n;
        }

        if (n == 0) {
            bb.tail("call void {}({, })", emmited_callee, args);
        } else {
            auto name   = "%" + app->unique_name() + ".ret";
            auto ret_ty = convert_ret_pi(ret_lam->type());
            bb.tail("{} = call {} {}({, })", name, ret_ty, emmited_callee, args);

            for (size_t i = 1, e = ret_lam->num_vars(); i != e; ++i) {
                auto phi   = ret_lam->var(i);
                auto namei = name;
                if (e > 2) {
                    namei += '.' + std::to_string(i - 1);
                    bb.tail("{} = extractvalue {} {}, {}", namei, ret_ty, name, i - 1);
                }
                assert(!match<mem::M>(phi->type()));
                lam2bb_[ret_lam].phis[phi].emplace_back(namei, id(lam, true));
                locals_[phi] = id(phi);
            }
        }

        return bb.tail("br label {}", id(ret_lam));
    }
}

std::string Emitter::emit_bb(BB& bb, const Def* def) {
    if (def->isa<Var>()) return {};
    if (auto lam = def->isa<Lam>()) return id(lam);

    auto name = id(def);
    std::string op;

    auto emit_tuple = [&](const Def* tuple) {
        if (is_const(tuple)) {
            bool is_array = tuple->type()->isa<Arr>();

            std::string s;
            s += is_array ? "[" : "{";
            auto sep = "";
            for (size_t i = 0, n = tuple->num_projs(); i != n; ++i) {
                auto e = tuple->proj(n, i);
                if (auto elem = emit_unsafe(e); !elem.empty()) {
                    auto elem_t = convert(e->type());
                    s += sep + elem_t + " " + elem;
                    sep = ", ";
                }
            }

            return s += is_array ? "]" : "}";
        }

        std::string prev = "undef";
        auto t           = convert(tuple->type());
        for (size_t i = 0, n = tuple->num_projs(); i != n; ++i) {
            auto e = tuple->proj(n, i);
            if (auto elem = emit_unsafe(e); !elem.empty()) {
                auto elem_t = convert(e->type());
                auto namei  = name + "." + std::to_string(i);
                prev        = bb.assign(namei, "insertvalue {} {}, {} {}, {}", t, prev, elem_t, elem, i);
            }
        }
        return prev;
    };

    if (auto lit = def->isa<Lit>()) {
        if (lit->type()->isa<Nat>()) {
            return std::to_string(lit->get<nat_t>());
        } else if (auto idx = lit->type()->isa<Idx>()) {
            auto size = idx->size();
            if (size->isa<Top>()) return std::to_string(lit->get<nat_t>());
            if (auto mod = size2bitwidth(as_lit(size))) {
                switch (*mod) {
                    // clang-format off
                    case  0: return {};
                    case  1: return std::to_string(lit->get< u1>());
                    case  2:
                    case  4:
                    case  8: return std::to_string(lit->get< u8>());
                    case 16: return std::to_string(lit->get<u16>());
                    case 32: return std::to_string(lit->get<u32>());
                    case 64: return std::to_string(lit->get<u64>());
                    // clang-format on
                    default: return std::to_string(lit->get<u64>());
                }
            } else {
                return std::to_string(lit->get<u64>());
            }
        } else if (auto real = isa<Tag::Real>(lit->type())) {
            std::stringstream s;
            u64 hex;

            switch (as_lit<nat_t>(real->arg())) {
                case 16:
                    s << "0xH" << std::setfill('0') << std::setw(4) << std::right << std::hex << lit->get<u16>();
                    return s.str();
                case 32: {
                    hex = std::bit_cast<u64>(r64(lit->get<r32>()));
                    break;
                }
                case 64: hex = lit->get<u64>(); break;
                default: unreachable();
            }

            s << "0x" << std::setfill('0') << std::setw(16) << std::right << std::hex << hex;
            return s.str();
        }
        unreachable();
    } else if (def->isa<Bot>()) {
        return "undef";
    } else if (auto bit = isa<Tag::Bit>(def)) {
        auto [a, b] = bit->args<2>([this](auto def) { return emit(def); });
        auto t      = convert(bit->type());

        auto neg = [&](std::string_view x) { return bb.assign(name + ".neg", "xor {} 0, {}", t, x); };

        switch (bit.sub()) {
            // clang-format off
            case Bit::_and: return bb.assign(name, "and {} {}, {}", t, a, b);
            case Bit:: _or: return bb.assign(name, "or  {} {}, {}", t, a, b);
            case Bit::_xor: return bb.assign(name, "xor {} {}, {}", t, a, b);
            case Bit::nand: return neg(bb.assign(name, "and {} {}, {}", t, a, b));
            case Bit:: nor: return neg(bb.assign(name, "or  {} {}, {}", t, a, b));
            case Bit::nxor: return neg(bb.assign(name, "xor {} {}, {}", t, a, b));
            case Bit:: iff: return bb.assign(name, "and {} {}, {}", neg(a), b);
            case Bit::niff: return bb.assign(name, "or  {} {}, {}", neg(a), b);
            // clang-format on
            default: unreachable();
        }
    } else if (auto bit = match<core::bit2>(def)) {
        auto [a, b] = bit->args<2>([this](auto def) { return emit(def); });
        auto t      = convert(bit->type());

        auto neg = [&](std::string_view x) { return bb.assign(name + ".neg", "xor {} 0, {}", t, x); };

        switch (bit.flags()) {
            // clang-format off
            case core::bit2::_and: return bb.assign(name, "and {} {}, {}", t, a, b);
            case core::bit2:: _or: return bb.assign(name, "or  {} {}, {}", t, a, b);
            case core::bit2::_xor: return bb.assign(name, "xor {} {}, {}", t, a, b);
            case core::bit2::nand: return neg(bb.assign(name, "and {} {}, {}", t, a, b));
            case core::bit2:: nor: return neg(bb.assign(name, "or  {} {}, {}", t, a, b));
            case core::bit2::nxor: return neg(bb.assign(name, "xor {} {}, {}", t, a, b));
            case core::bit2:: iff: return bb.assign(name, "and {} {}, {}", neg(a), b);
            case core::bit2::niff: return bb.assign(name, "or  {} {}, {}", neg(a), b);
            // clang-format on
            default: unreachable();
        }
    } else if (auto shr = isa<Tag::Shr>(def)) {
        auto [a, b] = shr->args<2>([this](auto def) { return emit(def); });
        auto t      = convert(shr->type());

        switch (shr.sub()) {
            case Shr::ashr: op = "ashr"; break;
            case Shr::lshr: op = "lshr"; break;
            default: unreachable();
        }

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto shr = match<core::shr>(def)) {
        auto [a, b] = shr->args<2>([this](auto def) { return emit(def); });
        auto t      = convert(shr->type());

        switch (shr.flags()) {
            case core::shr::ashr: op = "ashr"; break;
            case core::shr::lshr: op = "lshr"; break;
            default: unreachable();
        }

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto wrap = isa<Tag::Wrap>(def)) {
        auto [a, b]        = wrap->args<2>([this](auto def) { return emit(def); });
        auto t             = convert(wrap->type());
        auto [mode, width] = wrap->decurry()->args<2>(as_lit<nat_t>);

        switch (wrap.sub()) {
            case Wrap::add: op = "add"; break;
            case Wrap::sub: op = "sub"; break;
            case Wrap::mul: op = "mul"; break;
            case Wrap::shl: op = "shl"; break;
            default: unreachable();
        }

        if (mode & WMode::nuw) op += " nuw";
        if (mode & WMode::nsw) op += " nsw";

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto wrap = match<core::wrap>(def)) {
        auto [a, b]        = wrap->args<2>([this](auto def) { return emit(def); });
        auto t             = convert(wrap->type());
        auto [mode, width] = wrap->decurry()->args<2>(as_lit<nat_t>);

        switch (wrap.flags()) {
            case core::wrap::add: op = "add"; break;
            case core::wrap::sub: op = "sub"; break;
            case core::wrap::mul: op = "mul"; break;
            case core::wrap::shl: op = "shl"; break;
            default: unreachable();
        }

        if (mode & WMode::nuw) op += " nuw";
        if (mode & WMode::nsw) op += " nsw";

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto div = match<core::div>(def)) {
        auto [m, x, y] = div->args<3>();
        auto t         = convert(x->type());
        emit_unsafe(m);
        auto a = emit(x);
        auto b = emit(y);

        switch (div.flags()) {
            case core::div::sdiv: op = "sdiv"; break;
            case core::div::udiv: op = "udiv"; break;
            case core::div::srem: op = "srem"; break;
            case core::div::urem: op = "urem"; break;
            default: unreachable();
        }

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto rop = isa<Tag::ROp>(def)) {
        auto [a, b]        = rop->args<2>([this](auto def) { return emit(def); });
        auto t             = convert(rop->type());
        auto [mode, width] = rop->decurry()->args<2>(as_lit<nat_t>);

        switch (rop.sub()) {
            case ROp::add: op = "fadd"; break;
            case ROp::sub: op = "fsub"; break;
            case ROp::mul: op = "fmul"; break;
            case ROp::div: op = "fdiv"; break;
            case ROp::rem: op = "frem"; break;
            default: unreachable();
        }

        if (mode == RMode::fast)
            op += " fast";
        else {
            // clang-format off
            if (mode & RMode::nnan    ) op += " nnan";
            if (mode & RMode::ninf    ) op += " ninf";
            if (mode & RMode::nsz     ) op += " nsz";
            if (mode & RMode::arcp    ) op += " arcp";
            if (mode & RMode::contract) op += " contract";
            if (mode & RMode::afn     ) op += " afn";
            if (mode & RMode::reassoc ) op += " reassoc";
            // clang-format on
        }

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto icmp = isa<Tag::ICmp>(def)) {
        auto [a, b] = icmp->args<2>([this](auto def) { return emit(def); });
        auto t      = convert(icmp->arg(0)->type());
        op          = "icmp ";

        switch (icmp.sub()) {
            // clang-format off
            case ICmp::e:   op += "eq" ; break;
            case ICmp::ne:  op += "ne" ; break;
            case ICmp::sg:  op += "sgt"; break;
            case ICmp::sge: op += "sge"; break;
            case ICmp::sl:  op += "slt"; break;
            case ICmp::sle: op += "sle"; break;
            case ICmp::ug:  op += "ugt"; break;
            case ICmp::uge: op += "uge"; break;
            case ICmp::ul:  op += "ult"; break;
            case ICmp::ule: op += "ule"; break;
            // clang-format on
            default: unreachable();
        }

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto icmp = match<core::icmp>(def)) {
        auto [a, b] = icmp->args<2>([this](auto def) { return emit(def); });
        auto t      = convert(icmp->arg(0)->type());
        op          = "icmp ";

        switch (icmp.flags()) {
            // clang-format off
            case core::icmp::e:   op += "eq" ; break;
            case core::icmp::ne:  op += "ne" ; break;
            case core::icmp::sg:  op += "sgt"; break;
            case core::icmp::sge: op += "sge"; break;
            case core::icmp::sl:  op += "slt"; break;
            case core::icmp::sle: op += "sle"; break;
            case core::icmp::ug:  op += "ugt"; break;
            case core::icmp::uge: op += "uge"; break;
            case core::icmp::ul:  op += "ult"; break;
            case core::icmp::ule: op += "ule"; break;
            // clang-format on
            default: unreachable();
        }

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto rcmp = isa<Tag::RCmp>(def)) {
        auto [a, b] = rcmp->args<2>([this](auto def) { return emit(def); });
        auto t      = convert(rcmp->arg(0)->type());
        op          = "fcmp ";

        switch (rcmp.sub()) {
            // clang-format off
            case RCmp::  e: op += "oeq"; break;
            case RCmp::  l: op += "olt"; break;
            case RCmp:: le: op += "ole"; break;
            case RCmp::  g: op += "ogt"; break;
            case RCmp:: ge: op += "oge"; break;
            case RCmp:: ne: op += "one"; break;
            case RCmp::  o: op += "ord"; break;
            case RCmp::  u: op += "uno"; break;
            case RCmp:: ue: op += "ueq"; break;
            case RCmp:: ul: op += "ult"; break;
            case RCmp::ule: op += "ule"; break;
            case RCmp:: ug: op += "ugt"; break;
            case RCmp::uge: op += "uge"; break;
            case RCmp::une: op += "une"; break;
            // clang-format on
            default: unreachable();
        }

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto conv = isa<Tag::Conv>(def)) {
        auto src   = emit(conv->arg());
        auto src_t = convert(conv->arg()->type());
        auto dst_t = convert(conv->type());

        auto size2width = [&](const Def* type) {
            if (auto idx = type->isa<Idx>()) {
                if (idx->size()->isa<Top>()) return 64_u64;
                if (auto width = size2bitwidth(as_lit(idx->size()))) return *width;
                return 64_u64;
            }
            return as_lit(as<Tag::Real>(type)->arg());
        };

        nat_t s_src = size2width(conv->arg()->type());
        nat_t s_dst = size2width(conv->type());

        // this might happen when casting from int top to i64
        if (s_src == s_dst && (conv.sub() == Conv::s2s || conv.sub() == Conv::u2u)) return src;

        switch (conv.sub()) {
            // clang-format off
            case Conv::s2s: op = s_src < s_dst ? "sext"  : "trunc";   break;
            case Conv::u2u: op = s_src < s_dst ? "zext"  : "trunc";   break;
            case Conv::r2r: op = s_src < s_dst ? "fpext" : "fptrunc"; break;
            case Conv::s2r: op = "sitofp"; break;
            case Conv::u2r: op = "uitofp"; break;
            case Conv::r2s: op = "fptosi"; break;
            case Conv::r2u: op = "fptoui"; break;
            // clang-format on
            default: unreachable();
        }

        return bb.assign(name, "{} {} {} to {}", op, src_t, src, dst_t);
    } else if (auto conv = match<core::conv>(def)) {
        auto src   = emit(conv->arg());
        auto src_t = convert(conv->arg()->type());
        auto dst_t = convert(conv->type());

        auto size2width = [&](const Def* type) {
            if (auto idx = type->isa<Idx>()) {
                if (idx->size()->isa<Top>()) return 64_u64;
                if (auto width = size2bitwidth(as_lit(idx->size()))) return *width;
                return 64_u64;
            }
            return as_lit(as<Tag::Real>(type)->arg());
        };

        nat_t s_src = size2width(conv->arg()->type());
        nat_t s_dst = size2width(conv->type());

        // this might happen when casting from int top to i64
        if (s_src == s_dst && (conv.flags() == core::conv::s2s || conv.flags() == core::conv::u2u)) return src;

        switch (conv.flags()) {
            // clang-format off
            case core::conv::s2s: op = s_src < s_dst ? "sext"  : "trunc";   break;
            case core::conv::u2u: op = s_src < s_dst ? "zext"  : "trunc";   break;
            case core::conv::r2r: op = s_src < s_dst ? "fpext" : "fptrunc"; break;
            case core::conv::s2r: op = "sitofp"; break;
            case core::conv::u2r: op = "uitofp"; break;
            case core::conv::r2s: op = "fptosi"; break;
            case core::conv::r2u: op = "fptoui"; break;
            // clang-format on
            default: unreachable();
        }

        return bb.assign(name, "{} {} {} to {}", op, src_t, src, dst_t);
    } else if (auto bitcast = isa<Tag::Bitcast>(def)) {
        auto dst_type_ptr = match<mem::Ptr>(bitcast->type());
        auto src_type_ptr = match<mem::Ptr>(bitcast->arg()->type());
        auto src          = emit(bitcast->arg());
        auto src_t        = convert(bitcast->arg()->type());
        auto dst_t        = convert(bitcast->type());

        if (auto lit = isa_lit(bitcast->arg()); lit && *lit == 0) return "zeroinitializer";
        // clang-format off
        if (src_type_ptr && dst_type_ptr) return bb.assign(name,  "bitcast {} {} to {}", src_t, src, dst_t);
        if (src_type_ptr)                 return bb.assign(name, "ptrtoint {} {} to {}", src_t, src, dst_t);
        if (dst_type_ptr)                 return bb.assign(name, "inttoptr {} {} to {}", src_t, src, dst_t);
        // clang-format on

        auto size2width = [&](const Def* type) {
            if (type->isa<Nat>()) return 64_u64;
            if (auto idx = type->isa<Idx>()) {
                if (idx->size()->isa<Top>() || !idx->size()->isa<Lit>()) return 64_u64;
                if (auto width = size2bitwidth(as_lit(idx->size()))) return std::bit_ceil(*width);
                return 64_u64;
            }
            return 0_u64;
        };

        auto src_size = size2width(bitcast->arg()->type());
        auto dst_size = size2width(bitcast->type());

        op = "bitcast";
        if (src_size && dst_size) {
            if (src_size == dst_size) return src;
            op = (src_size < dst_size) ? "zext" : "trunc";
        }
        return bb.assign(name, "{} {} {} to {}", op, src_t, src, dst_t);
    } else if (auto bitcast = match<core::bitcast>(def)) {
        auto dst_type_ptr = match<mem::Ptr>(bitcast->type());
        auto src_type_ptr = match<mem::Ptr>(bitcast->arg()->type());
        auto src          = emit(bitcast->arg());
        auto src_t        = convert(bitcast->arg()->type());
        auto dst_t        = convert(bitcast->type());

        if (auto lit = isa_lit(bitcast->arg()); lit && *lit == 0) return "zeroinitializer";
        // clang-format off
        if (src_type_ptr && dst_type_ptr) return bb.assign(name,  "bitcast {} {} to {}", src_t, src, dst_t);
        if (src_type_ptr)                 return bb.assign(name, "ptrtoint {} {} to {}", src_t, src, dst_t);
        if (dst_type_ptr)                 return bb.assign(name, "inttoptr {} {} to {}", src_t, src, dst_t);
        // clang-format on

        auto size2width = [&](const Def* type) {
            if (type->isa<Nat>()) return 64_u64;
            if (auto idx = type->isa<Idx>()) {
                if (idx->size()->isa<Top>() || !idx->size()->isa<Lit>()) return 64_u64;
                if (auto width = size2bitwidth(as_lit(idx->size()))) return std::bit_ceil(*width);
                return 64_u64;
            }
            return 0_u64;
        };

        auto src_size = size2width(bitcast->arg()->type());
        auto dst_size = size2width(bitcast->type());

        op = "bitcast";
        if (src_size && dst_size) {
            if (src_size == dst_size) return src;
            op = (src_size < dst_size) ? "zext" : "trunc";
        }
        return bb.assign(name, "{} {} {} to {}", op, src_t, src, dst_t);
    } else if (auto lea = match<mem::lea>(def)) {
        auto [ptr, i] = lea->args<2>();
        auto ll_ptr   = emit(ptr);
        auto pointee  = match<mem::Ptr, false>(ptr->type())->arg(0);
        auto t        = convert(pointee);
        auto p        = convert(ptr->type());
        if (pointee->isa<Sigma>())
            return bb.assign(name, "getelementptr inbounds {}, {} {}, i64 0, i32 {}", t, p, ll_ptr, as_lit<u64>(i));

        assert(pointee->isa<Arr>());
        auto ll_i = emit(i);
        auto i_t  = convert(i->type());

        if (auto idx = i->type()->isa<Idx>()) {
            auto size = idx->size();
            if (auto s = isa_lit(size); s && *s == 2) { // mod(2) = width(1)
                ll_i = bb.assign(name + ".8", "zext i1 {} to i8", ll_i);
                i_t  = "i8";
            }
        }

        return bb.assign(name, "getelementptr inbounds {}, {} {}, i64 0, {} {}", t, p, ll_ptr, i_t, ll_i);
    } else if (auto trait = isa<Tag::Trait>(def)) {
        unreachable();
    } else if (auto malloc = match<mem::malloc>(def)) {
        emit_unsafe(malloc->arg(0));
        auto size  = emit(malloc->arg(1));
        auto ptr_t = convert(match<mem::Ptr, false>(def->proj(1)->type()));
        bb.assign(name + ".i8", "call i8* @malloc(i64 {})", size);
        return bb.assign(name, "bitcast i8* {} to {}", name + ".i8", ptr_t);
    } else if (auto mslot = match<mem::mslot>(def)) {
        emit_unsafe(mslot->arg(0));
        // TODO array with size
        // auto size = emit(mslot->arg(1));
        auto [pointee, addr_space] = mslot->decurry()->args<2>();
        print(lam2bb_[entry_].body().emplace_front(), "{} = alloca {}", name, convert(pointee));
        return name;
    } else if (auto load = match<mem::load>(def)) {
        emit_unsafe(load->arg(0));
        auto ptr       = emit(load->arg(1));
        auto ptr_t     = convert(load->arg(1)->type());
        auto pointee_t = convert(match<mem::Ptr, false>(load->arg(1)->type())->arg(0));
        return bb.assign(name, "load {}, {} {}", pointee_t, ptr_t, ptr);
    } else if (auto store = match<mem::store>(def)) {
        emit_unsafe(store->arg(0));
        auto ptr   = emit(store->arg(1));
        auto val   = emit(store->arg(2));
        auto ptr_t = convert(store->arg(1)->type());
        auto val_t = convert(store->arg(2)->type());
        print(bb.body().emplace_back(), "store {} {}, {} {}", val_t, val, ptr_t, ptr);
        return {};
    } else if (auto q = match<clos::alloc_jmpbuf>(def)) {
        emit_unsafe(q->arg());
        auto size = name + ".size";
        bb.assign(size, "call i64 @jmpbuf_size()");
        return bb.assign(name, "alloca i8, i64 {}", size);
    } else if (auto setjmp = match<clos::setjmp>(def)) {
        auto [mem, jmpbuf] = setjmp->arg()->projs<2>();
        emit_unsafe(mem);
        auto emitted_jb = emit(jmpbuf);
        return bb.assign(name, "call i32 @_setjmp(i8* {})", emitted_jb);
    } else if (auto tuple = def->isa<Tuple>()) {
        return emit_tuple(tuple);
    } else if (auto pack = def->isa<Pack>()) {
        if (auto lit = isa_lit(pack->body()); lit && *lit == 0) return "zeroinitializer";
        return emit_tuple(pack);
    } else if (auto extract = def->isa<Extract>()) {
        auto tuple = extract->tuple();
        auto index = extract->index();

#if 0
        // TODO this was von closure-conv branch which I need to double-check
        if (tuple->isa<Var>()) {
            // computing the index may crash, so we bail out
            assert(isa<Tag::Mem>(extract->type()) && "only mem-var should not be mapped");
            return {};
        }
#endif

        auto ll_tup = emit_unsafe(tuple);

        // this exact location is important: after emitting the tuple -> ordering of mem ops
        // before emitting the index, as it might be a weird value for mem vars.
        if (match<mem::M>(extract->type())) return {};

        auto ll_idx = emit_unsafe(index);

        if (tuple->num_projs() == 2) {
            if (match<mem::M>(tuple->proj(2, 0_s)->type())) return ll_tup;
            if (match<mem::M>(tuple->proj(2, 1_s)->type())) return ll_tup;
        }

        auto tup_t = convert(tuple->type());
        if (isa_lit(index)) {
            assert(!ll_tup.empty());
            return bb.assign(name, "extractvalue {} {}, {}", tup_t, ll_tup, ll_idx);
        } else {
            auto elem_t = convert(extract->type());
            print(lam2bb_[entry_].body().emplace_front(),
                  "{}.alloca = alloca {} ; copy to alloca to emulate extract with store + gep + load", name, tup_t);
            print(bb.body().emplace_back(), "store {} {}, {}* {}.alloca", tup_t, ll_tup, tup_t, name);
            print(bb.body().emplace_back(), "{}.gep = getelementptr inbounds {}, {}* {}.alloca, i64 0, i64 {}", name,
                  tup_t, tup_t, name, ll_idx);
            return bb.assign(name, "load {}, {}* {}.gep", elem_t, elem_t, name);
        }
    } else if (auto insert = def->isa<Insert>()) {
        auto tuple = emit(insert->tuple());
        auto index = emit(insert->index());
        auto value = emit(insert->value());
        auto tup_t = convert(insert->tuple()->type());
        auto val_t = convert(insert->value()->type());
        return bb.assign(name, "insertvalue {} {}, {} {}, {}", tup_t, tuple, val_t, value, index);
    } else if (auto global = def->isa<Global>()) {
        auto init                  = emit(global->init());
        auto [pointee, addr_space] = match<mem::Ptr, false>(global->type())->args<2>();
        print(vars_decls_, "{} = global {} {}\n", name, convert(pointee), init);
        return globals_[global] = name;
    }

    unreachable(); // not yet implemented
}

void emit(World& world, std::ostream& ostream) {
    Emitter emitter(world, ostream);
    emitter.run();
}

int compile(World& world, std::string name) {
#ifdef _WIN32
    auto exe = name + ".exe"s;
#else
    auto exe = name;
#endif
    return compile(world, name + ".ll"s, exe);
}

int compile(World& world, std::string ll, std::string out) {
    std::ofstream ofs(ll);
    emit(world, ofs);
    ofs.close();
    auto cmd = fmt("clang \"{}\" -o \"{}\" -Wno-override-module", ll, out);
    return sys::system(cmd);
}

int compile_and_run(World& world, std::string name, std::string args) {
    if (compile(world, name) == 0) return sys::run(name, args);
    throw std::runtime_error("compilation failed");
}

} // namespace thorin::ll
