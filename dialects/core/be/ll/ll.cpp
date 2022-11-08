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
#include "dialects/math/math.h"
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

    template<class... Args>
    void declare(const char* s, Args&&... args) {
        std::ostringstream decl;
        print(decl << "declare ", s, std::forward<Args&&>(args)...);
        decls_.emplace(decl.str());
    }

private:
    std::string id(const Def*, bool force_bb = false) const;
    std::string convert(const Def*);
    std::string convert_ret_pi(const Pi*);

    absl::btree_set<std::string> decls_;
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
    } else if (auto size = Idx::size(type)) {
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
    } else if (auto w = math::isa_f(type)) {
        switch (*w) {
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

    ostream() << type_decls_.str() << '\n';
    for (auto&& decl : decls_) ostream() << decl << '\n';
    ostream() << func_decls_.str() << '\n';
    ostream() << vars_decls_.str() << '\n';
    ostream() << func_impls_.str() << '\n';
}

void Emitter::emit_imported(Lam* lam) {
    // TODO merge with declare method
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
        declare("void @longjmp(i8*, i32) noreturn");

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

            for (size_t i = 0, e = ret_lam->num_vars(); i != e; ++i) {
                auto phi = ret_lam->var(i);
                if (match<mem::M>(phi->type())) continue;

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

static const char* math_suffix(const Def* type) {
    if (auto s = math::isa_f(type)) {
        switch (*s) {
            case 32: return "f";
            case 64: return "";
        }
    }
    err("unsupported foating point type '{}'", type);
}
static const char* llvm_suffix(const Def* type) {
    if (auto s = math::isa_f(type)) {
        switch (*s) {
            case 16: return ".f16";
            case 32: return ".f32";
            case 64: return ".f64";
        }
    }
    err("unsupported foating point type '{}'", type);
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
        } else if (auto size = Idx::size(lit->type())) {
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
        } else if (auto w = math::isa_f(lit->type())) {
            std::stringstream s;
            u64 hex;

            switch (*w) {
                case 16:
                    s << "0xH" << std::setfill('0') << std::setw(4) << std::right << std::hex << lit->get<u16>();
                    return s.str();
                case 32: {
                    hex = std::bit_cast<u64>(f64(lit->get<f32>()));
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
            assert(match<core::Mem>(extract->type()) && "only mem-var should not be mapped");
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
        } else if (index->type() == index->world().type_bool()) {
            auto elem_t                = convert(extract->type());
            auto [false_def, true_def] = tuple->ops<2>();

            auto false_val = emit_unsafe(false_def);
            auto true_val  = emit_unsafe(true_def);

            return bb.assign(name, "select fast i1 {}, {} {}, {} {} ", ll_idx, elem_t, true_val, elem_t, false_val);
        } else {
            auto elem_t  = convert(extract->type());
            auto index_t = convert(index->type());
            print(lam2bb_[entry_].body().emplace_front(),
                  "{}.alloca = alloca {} ; copy to alloca to emulate extract with store + gep + load", name, tup_t);
            print(bb.body().emplace_back(), "store {} {}, {}* {}.alloca", tup_t, ll_tup, tup_t, name);
            print(bb.body().emplace_back(), "{}.zext = zext {} {} to i64", ll_idx, index_t, ll_idx);
            print(bb.body().emplace_back(), "{}.gep = getelementptr inbounds {}, {}* {}.alloca, i64 0, i64 {}.zext",
                  name, tup_t, tup_t, name, ll_idx);
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
        auto [pointee, addr_space] = force<mem::Ptr>(global->type())->args<2>();
        print(vars_decls_, "{} = global {} {}\n", name, convert(pointee), init);
        return globals_[global] = name;
    } else if (auto nop = match<core::nop>(def)) {
        auto [a, b] = nop->args<2>([this](auto def) { return emit(def); });

        switch (nop.id()) {
            case core::nop::add: op = "add"; break;
            case core::nop::mul: op = "mul"; break;
        }

        return bb.assign(name, "{} nsw nuw i64 {}, {}", op, a, b);
    } else if (auto ncmp = match<core::ncmp>(def)) {
        auto [a, b] = ncmp->args<2>([this](auto def) { return emit(def); });
        op          = "icmp ";

        switch (ncmp.id()) {
            // clang-format off
            case core::ncmp::e:  op += "eq" ; break;
            case core::ncmp::ne: op += "ne" ; break;
            case core::ncmp::g:  op += "ugt"; break;
            case core::ncmp::ge: op += "uge"; break;
            case core::ncmp::l:  op += "ult"; break;
            case core::ncmp::le: op += "ule"; break;
            // clang-format on
            default: unreachable();
        }

        return bb.assign(name, "{} i64 {}, {}", op, a, b);
    } else if (auto bit1 = match<core::bit1>(def)) {
        assert(bit1.id() == core::bit1::neg);
        auto x = emit(bit1->arg());
        auto t = convert(bit1->type());
        return bb.assign(name, "xor {} -1, {}", t, x);
    } else if (auto bit2 = match<core::bit2>(def)) {
        auto [a, b] = bit2->args<2>([this](auto def) { return emit(def); });
        auto t      = convert(bit2->type());

        auto neg = [&](std::string_view x) { return bb.assign(name + ".neg", "xor {} -1, {}", t, x); };

        switch (bit2.id()) {
            // clang-format off
            case core::bit2::and_: return bb.assign(name, "and {} {}, {}", t, a, b);
            case core::bit2:: or_: return bb.assign(name, "or  {} {}, {}", t, a, b);
            case core::bit2::xor_: return bb.assign(name, "xor {} {}, {}", t, a, b);
            case core::bit2::nand: return neg(bb.assign(name, "and {} {}, {}", t, a, b));
            case core::bit2:: nor: return neg(bb.assign(name, "or  {} {}, {}", t, a, b));
            case core::bit2::nxor: return neg(bb.assign(name, "xor {} {}, {}", t, a, b));
            case core::bit2:: iff: return bb.assign(name, "and {} {}, {}", neg(a), b);
            case core::bit2::niff: return bb.assign(name, "or  {} {}, {}", neg(a), b);
            // clang-format on
            default: unreachable();
        }
    } else if (auto shr = match<core::shr>(def)) {
        auto [a, b] = shr->args<2>([this](auto def) { return emit(def); });
        auto t      = convert(shr->type());

        switch (shr.id()) {
            case core::shr::a: op = "ashr"; break;
            case core::shr::l: op = "lshr"; break;
        }

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto wrap = match<core::wrap>(def)) {
        auto [a, b]        = wrap->args<2>([this](auto def) { return emit(def); });
        auto t             = convert(wrap->type());
        auto [mode, width] = wrap->decurry()->args<2>(as_lit<nat_t>);

        switch (wrap.id()) {
            case core::wrap::add: op = "add"; break;
            case core::wrap::sub: op = "sub"; break;
            case core::wrap::mul: op = "mul"; break;
            case core::wrap::shl: op = "shl"; break;
        }

        if (mode & core::Mode::nuw) op += " nuw";
        if (mode & core::Mode::nsw) op += " nsw";

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto div = match<core::div>(def)) {
        auto [m, x, y] = div->args<3>();
        auto t         = convert(x->type());
        emit_unsafe(m);
        auto a = emit(x);
        auto b = emit(y);

        switch (div.id()) {
            case core::div::sdiv: op = "sdiv"; break;
            case core::div::udiv: op = "udiv"; break;
            case core::div::srem: op = "srem"; break;
            case core::div::urem: op = "urem"; break;
        }

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto icmp = match<core::icmp>(def)) {
        auto [a, b] = icmp->args<2>([this](auto def) { return emit(def); });
        auto t      = convert(icmp->arg(0)->type());
        op          = "icmp ";

        switch (icmp.id()) {
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
    } else if (auto conv = match<core::conv>(def)) {
        auto src   = emit(conv->arg());
        auto src_t = convert(conv->arg()->type());
        auto dst_t = convert(conv->type());

        auto size2width = [&](const Def* type) {
            auto size = Idx::size(type);
            if (size->isa<Top>()) return 64_u64;
            if (auto width = size2bitwidth(as_lit(size))) return *width;
            return 64_u64;
        };

        nat_t s_src = size2width(conv->arg()->type());
        nat_t s_dst = size2width(conv->type());

        // this might happen when casting from int top to i64
        if (s_src == s_dst && (conv.id() == core::conv::s2s || conv.id() == core::conv::u2u)) return src;

        switch (conv.id()) {
            case core::conv::s2s: op = s_src < s_dst ? "sext" : "trunc"; break;
            case core::conv::u2u: op = s_src < s_dst ? "zext" : "trunc"; break;
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
            if (auto size = Idx::size(type)) {
                if (size->isa<Top>() || !size->isa<Lit>()) return 64_u64;
                if (auto width = size2bitwidth(as_lit(size))) return std::bit_ceil(*width);
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
        auto pointee  = force<mem::Ptr>(ptr->type())->arg(0);
        auto t        = convert(pointee);
        auto p        = convert(ptr->type());
        if (pointee->isa<Sigma>())
            return bb.assign(name, "getelementptr inbounds {}, {} {}, i64 0, i32 {}", t, p, ll_ptr, as_lit<u64>(i));

        assert(pointee->isa<Arr>());
        auto ll_i = emit(i);
        auto i_t  = convert(i->type());

        if (auto size = Idx::size(i->type())) {
            if (auto s = isa_lit(size); s && *s == 2) { // mod(2) = width(1)
                ll_i = bb.assign(name + ".8", "zext i1 {} to i8", ll_i);
                i_t  = "i8";
            }
        }

        return bb.assign(name, "getelementptr inbounds {}, {} {}, i64 0, {} {}", t, p, ll_ptr, i_t, ll_i);
    } else if (match<core::trait>(def)) {
        unreachable();
    } else if (auto malloc = match<mem::malloc>(def)) {
        declare("i8* @malloc(i64)");

        emit_unsafe(malloc->arg(0));
        auto size  = emit(malloc->arg(1));
        auto ptr_t = convert(force<mem::Ptr>(def->proj(1)->type()));
        bb.assign(name + ".i8", "call i8* @malloc(i64 {})", size);
        return bb.assign(name, "bitcast i8* {} to {}", name + ".i8", ptr_t);
    } else if (auto mslot = match<mem::mslot>(def)) {
        emit_unsafe(mslot->arg(0));
        // TODO array with size
        // auto size = emit(mslot->arg(1));
        auto [pointee, addr_space] = mslot->decurry()->args<2>();
        // print(bb.body().emplace_back(), "{} = alloca {}", name, convert(pointee));
        print(lam2bb_[entry_].body().emplace_front(), "{} = alloca {}", name, convert(pointee));
        return name;
    } else if (auto free = match<mem::free>(def)) {
        declare("void @free(i8*)");

        emit_unsafe(free->arg(0));
        auto ptr   = emit(free->arg(1));
        auto ptr_t = convert(force<mem::Ptr>(free->arg(1)->type()));

        bb.assign(name + ".i8", "bitcast {} {} to i8*", ptr_t, ptr);
        bb.tail("call void @free(i8* {})", name + ".i8");
        return {};
    } else if (auto memset = match<mem::memset>(def)) {
        declare("void @llvm.memset.p0.i64(i8*, i8, i64, i1)");

        emit_unsafe(memset->arg(0));

        auto ptr  = emit(memset->arg(1));
        auto size = emit(memset->arg(2));
        auto val  = emit(memset->arg(3));

        print(bb.body().emplace_back(), "call void @llvm.memset.p0.i64(i8* {}, i8 {}, i64 {}, i1 false)", ptr, val,
              size);
        return {};
    } else if (auto load = match<mem::load>(def)) {
        emit_unsafe(load->arg(0));
        auto ptr       = emit(load->arg(1));
        auto ptr_t     = convert(load->arg(1)->type());
        auto pointee_t = convert(force<mem::Ptr>(load->arg(1)->type())->arg(0));
        return bb.assign(name, "load {}, {} {}", pointee_t, ptr_t, ptr);
    } else if (auto store = match<mem::store>(def)) {
        emit_unsafe(store->arg(0));
        auto ptr_def = store->arg(1);
        auto ptr_t   = convert(ptr_def->type());
        auto val_def = store->arg(2);

        auto ptr   = emit(ptr_def);
        auto val   = emit(val_def);
        auto val_t = convert(store->arg(2)->type());
        print(bb.body().emplace_back(), "store {} {}, {} {}", val_t, val, ptr_t, ptr);
        return {};
    } else if (auto q = match<clos::alloc_jmpbuf>(def)) {
        declare("i64 @jmpbuf_size()");

        emit_unsafe(q->arg());
        auto size = name + ".size";
        bb.assign(size, "call i64 @jmpbuf_size()");
        return bb.assign(name, "alloca i8, i64 {}", size);
    } else if (auto setjmp = match<clos::setjmp>(def)) {
        declare("i32 @_setjmp(i8*) returns_twice");

        auto [mem, jmpbuf] = setjmp->arg()->projs<2>();
        emit_unsafe(mem);
        auto emitted_jb = emit(jmpbuf);
        return bb.assign(name, "call i32 @_setjmp(i8* {})", emitted_jb);
    } else if (auto arith = match<math::arith>(def)) {
        auto [a, b] = arith->args<2>([this](auto def) { return emit(def); });
        auto t      = convert(arith->type());
        auto mode   = as_lit(arith->decurry()->arg());

        switch (arith.id()) {
            case math::arith::add: op = "fadd"; break;
            case math::arith::sub: op = "fsub"; break;
            case math::arith::mul: op = "fmul"; break;
            case math::arith::div: op = "fdiv"; break;
            case math::arith::rem: op = "frem"; break;
        }

        if (mode == math::Mode::fast)
            op += " fast";
        else {
            // clang-format off
            if (mode & math::Mode::nnan    ) op += " nnan";
            if (mode & math::Mode::ninf    ) op += " ninf";
            if (mode & math::Mode::nsz     ) op += " nsz";
            if (mode & math::Mode::arcp    ) op += " arcp";
            if (mode & math::Mode::contract) op += " contract";
            if (mode & math::Mode::afn     ) op += " afn";
            if (mode & math::Mode::reassoc ) op += " reassoc";
            // clang-format on
        }

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto tri = match<math::tri>(def)) {
        auto a = emit(tri->arg());
        auto t = convert(tri->type());

        std::string f;

        if (tri.id() == math::tri::sin) {
            f = "llvm.sin"s + llvm_suffix(tri->type());
        } else if (tri.id() == math::tri::cos) {
            f = "llvm.cos"s + llvm_suffix(tri->type());
        } else {
            if (tri.sub() & sub_t(math::tri::a)) f += "a";

            switch (math::tri((tri.id() & 0x3) | Axiom::Base<math::tri>)) {
                case math::tri::sin: f += "sin"; break;
                case math::tri::cos: f += "cos"; break;
                case math::tri::tan: f += "tan"; break;
                case math::tri::ahFF: err("this axiom is supposed to be unused");
                default: unreachable();
            }

            if (tri.sub() & sub_t(math::tri::h)) f += "h";
            f += math_suffix(tri->type());
        }

        declare("{} @{}({})", t, f, t);
        return bb.assign(name, "tail call {} @{}({} {})", t, f, t, a);
    } else if (auto extrema = match<math::extrema>(def)) {
        auto [a, b]   = extrema->args<2>([this](auto def) { return emit(def); });
        auto t        = convert(extrema->type());
        std::string f = "llvm.";
        switch (extrema.id()) {
            case math::extrema::minimum: f += "minimum"; break;
            case math::extrema::maximum: f += "maximum"; break;
            case math::extrema::minnum: f += "minnum"; break;
            case math::extrema::maxnum: f += "maxnum"; break;
        }
        f += llvm_suffix(extrema->type());

        declare("{} @{}({}, {})", t, f, t, t);
        return bb.assign(name, "tail call {} @{}({} {}, {} {})", t, f, t, a, t, b);
    } else if (auto pow = match<math::pow>(def)) {
        auto [a, b]   = pow->args<2>([this](auto def) { return emit(def); });
        auto t        = convert(pow->type());
        std::string f = "llvm.pow";
        f += llvm_suffix(pow->type());
        declare("{} @{}({}, {})", t, f, t, t);
        return bb.assign(name, "tail call {} @{}({} {}, {} {})", t, f, t, a, t, b);
    } else if (auto rt = match<math::rt>(def)) {
        auto a = emit(rt->arg());
        auto t = convert(rt->type());
        std::string f;
        if (rt.id() == math::rt::sq)
            f = "llvm.sqrt"s + llvm_suffix(rt->type());
        else
            f = "cbrt"s += math_suffix(rt->type());
        declare("{} @{}({})", t, f, t);
        return bb.assign(name, "tail call {} @{}({} {})", t, f, t, a);
    } else if (auto exp = match<math::exp>(def)) {
        auto a        = emit(exp->arg());
        auto t        = convert(exp->type());
        std::string f = "llvm.";
        // clang-format off
        switch (exp.id()) {
            case math::exp::exp:  f += "exp" ; break;
            case math::exp::exp2: f += "exp2"; break;
            case math::exp::log:  f += "log" ; break;
            case math::exp::log2: f += "log2"; break;
        }
        // clang-format on
        f += llvm_suffix(exp->type());
        declare("{} @{}({})", t, f, t);
        return bb.assign(name, "tail call {} @{}({} {})", t, f, t, a);
    } else if (auto er = match<math::er>(def)) {
        auto a        = emit(er->arg());
        auto t        = convert(er->type());
        std::string f = er.id() == math::er::f ? "erf" : "erfc";
        f += math_suffix(er->type());
        declare("{} @{}({})", t, f, t);
        return bb.assign(name, "tail call {} @{}({} {})", t, f, t, a);
    } else if (auto gamma = match<math::gamma>(def)) {
        auto a        = emit(gamma->arg());
        auto t        = convert(gamma->type());
        std::string f = gamma.id() == math::gamma::t ? "tgamma" : "lgamma";
        f += math_suffix(gamma->type());
        declare("{} @{}({})", t, f, t);
        return bb.assign(name, "tail call {} @{}({} {})", t, f, t, a);
    } else if (auto cmp = match<math::cmp>(def)) {
        auto [a, b] = cmp->args<2>([this](auto def) { return emit(def); });
        auto t      = convert(cmp->arg(0)->type());
        op          = "fcmp ";

        switch (cmp.id()) {
            // clang-format off
            case math::cmp::  e: op += "oeq"; break;
            case math::cmp::  l: op += "olt"; break;
            case math::cmp:: le: op += "ole"; break;
            case math::cmp::  g: op += "ogt"; break;
            case math::cmp:: ge: op += "oge"; break;
            case math::cmp:: ne: op += "one"; break;
            case math::cmp::  o: op += "ord"; break;
            case math::cmp::  u: op += "uno"; break;
            case math::cmp:: ue: op += "ueq"; break;
            case math::cmp:: ul: op += "ult"; break;
            case math::cmp::ule: op += "ule"; break;
            case math::cmp:: ug: op += "ugt"; break;
            case math::cmp::uge: op += "uge"; break;
            case math::cmp::une: op += "une"; break;
            // clang-format on
            default: unreachable();
        }

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto conv = match<math::conv>(def)) {
        auto src   = emit(conv->arg());
        auto src_t = convert(conv->arg()->type());
        auto dst_t = convert(conv->type());

        auto s_src = math::isa_f(conv->arg()->type());
        auto s_dst = math::isa_f(conv->type());

        switch (conv.id()) {
            case math::conv::f2f: op = s_src < s_dst ? "fpext" : "fptrunc"; break;
            case math::conv::s2f: op = "sitofp"; break;
            case math::conv::u2f: op = "uitofp"; break;
            case math::conv::f2s: op = "fptosi"; break;
            case math::conv::f2u: op = "fptoui"; break;
        }

        return bb.assign(name, "{} {} {} to {}", op, src_t, src, dst_t);
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
