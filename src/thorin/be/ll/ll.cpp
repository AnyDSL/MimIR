#include "thorin/be/ll/ll.h"

#include <deque>

#include "thorin/world.h"
#include "thorin/analyses/cfg.h"
#include "thorin/analyses/schedule.h"
#include "thorin/analyses/scope.h"
#include "thorin/be/emitter.h"
#include "thorin/util/stream.h"

namespace thorin::ll {

struct BB {
    BB() = default;

    Lam* lam = nullptr;
    DefMap<std::vector<std::pair<std::string, std::string>>> phis;
    std::array<std::deque<StringStream>, 3> parts;

    std::deque<StringStream>& head() { return parts[0]; }
    std::deque<StringStream>& body() { return parts[1]; }
    std::deque<StringStream>& tail() { return parts[2]; }

    template<class... Args> std::string assign(const std::string& name, const char* s, Args&&... args) {
        body().emplace_back().fmt("{} = ", name).fmt(s, std::forward<Args&&>(args)...);
        return name;
    }

    template<class... Args> void tail(const char* s, Args&&... args) {
        tail().emplace_back().fmt(s, std::forward<Args&&>(args)...);
    }

    friend void swap(BB& a, BB& b) {
        using std::swap;
        swap(a.lam,   b.lam);
        swap(a.parts, b.parts);
    }
};

class CodeGen : public Emitter<std::string, std::string, BB, CodeGen> {
public:
    CodeGen(World& world, Stream& stream)
        : world_(world)
        , debug_(false)
        , stream_(stream)
    {}

    World& world() const { return world_; }

    bool is_valid(const std::string& s) { return !s.empty(); }
    void emit_module();
    void emit_epilogue(Lam*);

    std::string emit_bb(BB&, const Def*);
    std::string emit_fun_decl(Lam*);
    std::string prepare(const Scope&);
    void prepare(Lam*, const std::string&);
    void finalize(const Scope&);

private:
    std::string convert(const Def*);
    std::string convert_ret_pi(const Pi*);

    World& world_;
    bool debug_;
    Stream& stream_;

    StringStream type_decls_;
    StringStream vars_decls_;
    StringStream func_decls_;
    StringStream func_impls_;
};

/*
 * convert
 */

std::string CodeGen::convert(const Def* type) {
    if (auto ll_type = types_.lookup(type)) return *ll_type;

    assert(!isa<Tag::Mem>(type));

    if (type->isa<Nat>()) {
        return types_[type] = "i32";
    } else if (isa<Tag::Int>(type)) {
        auto size = isa_sized_type(type);
        if (size->isa<Top>()) return types_[type] = "i64";
        if (auto width = mod2width(as_lit(size))) {
            switch (*width) {
                case  1: return types_[type] = "i1";
                case  2:
                case  4:
                case  8: return types_[type] = "i8";
                case 16: return types_[type] = "i16";
                case 32: return types_[type] = "i32";
                case 64: return types_[type] = "i64";
                default: THORIN_UNREACHABLE;
            }
        } else {
            return types_[type] = "i64";
        }
    } else if (auto real = isa<Tag::Real>(type)) {
        switch (as_lit<nat_t>(real->arg())) {
            case 16: return types_[type] = "half";
            case 32: return types_[type] = "float";
            case 64: return types_[type] = "double";
            default: THORIN_UNREACHABLE;
        }
    }

    StringStream s;
    std::string name;

    if (auto ptr = isa<Tag::Ptr>(type)) {
        auto [pointee, addr_space] = ptr->args<2>();
        // TODO addr_space
        s.fmt("{}*", convert(pointee));
    } else if (auto arr = type->isa<Arr>()) {
        auto elem_type = convert(arr->body());
        u64 size = 0;
        if (auto arity = isa_lit(arr->shape())) size = *arity;
        s.fmt("[{} x {}]", size, elem_type);
    } else if (auto pi = type->isa<Pi>()) {
        s.fmt("{} (", convert(pi->doms().back()->as<Pi>()->dom()));

        const char* sep = "";
        for (auto dom : pi->doms().skip_back()) {
            if (isa<Tag::Mem>(dom)) continue;
            s << sep << convert(dom);
            sep = ", ";
        }
    } else if (auto sigma = type->isa<Sigma>()) {
        if (sigma->isa_nom()) {
            name = sigma->name();
            types_[sigma] = name;
            s.fmt("{} = ", name);
        }
        const char* sep = "";
        for (auto t : sigma->ops()) {
            if (isa<Tag::Mem>(t)) continue;
            s << sep << convert(t);
            sep = ", ";
        }
    }

    if (name.empty()) return types_[type] = s.str();

    assert(!s.str().empty());
    type_decls_ << s.str();
    return types_[type] = name;
}

std::string CodeGen::convert_ret_pi(const Pi* pi) {
    switch (pi->num_doms()) {
        case 0:
            return "void";
        case 1:
            if (isa<Tag::Mem>(pi->dom())) return "void";
            return convert(pi->dom());
        case 2:
            if (isa<Tag::Mem>(pi->dom(0))) return convert(pi->dom(1));
            if (isa<Tag::Mem>(pi->dom(1))) return convert(pi->dom(0));
            [[fallthrough]];
        default:
            return convert(pi->dom());
    }
}

/*
 * emit
 */

void CodeGen::emit_module() {
    world_.visit([&](const Scope& scope) { emit_scope(scope); });

    stream_ << type_decls_.str() << '\n';
    stream_ << func_decls_.str() << '\n';
    stream_ << vars_decls_.str() << '\n';
    stream_ << func_impls_.str() << '\n';
}

std::string CodeGen::prepare(const Scope& scope) {
    auto lam = scope.entry()->as_nom<Lam>();

    func_impls_.fmt("define {} @{}(", convert_ret_pi(lam->type()->ret_pi()), lam->unique_name());

    const char* sep = "";
    for (auto var : lam->vars().skip_back()) {
        if (isa<Tag::Mem>(var->type())) continue;
        func_impls_.fmt("{}{} {}", sep, convert(var->type()), var->unique_name());
        sep = ", ";
    }

    func_impls_.fmt(") {{\n");

    return lam->unique_name();
}

void CodeGen::finalize(const Scope& scope) {
    for (auto& [lam, bb] : lam2bb_) {
        for (const auto& [phi, args] : bb.phis) {
            bb.head().emplace_back().fmt("{} = phi ", phi);
            const char* sep = "";
            for (const auto& [arg, pred] : args) {
                bb.head().back().fmt("{}[ {}, {} ]", sep, arg, pred);
                sep = ", ";
            }
        }
    }

    for (auto nom : schedule(scope)) {
        if (auto lam = nom->isa_nom<Lam>()) {
            auto& bb = lam2bb_[lam];
            func_impls_.fmt("{}:\t\n", lam->unique_name());

            for (const auto& part : bb.parts) {
                for (const auto& line : part)
                    (func_impls_ << line.str()).endl();
            }

            func_impls_.dedent().endl();
        }
    }

    func_impls_.fmt("\n}}\n");
}

void CodeGen::emit_epilogue(Lam* lam) {
    auto&& bb = lam2bb_[lam];
    auto app = lam->body()->as<App>();

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
            case 0: return bb.tail("ret");
            case 1: return bb.tail("ret {} {}", convert(types[0]), values[0]);
            default:
                auto tuple = convert(world().sigma(types));
                bb.tail("{} ret_val\n", tuple);
                for (size_t i = 0, e = types.size(); i != e; ++i)
                    bb.tail("ret_val.e{} = {};\n", i, values[i]);
                return bb.tail("ret ret_val");
        }
    } else if (auto ex = app->callee()->isa<Extract>()) {
        auto c = emit(ex->index());
        auto [f, t] = ex->tuple()->projs<2>([this](auto def) { return emit(def); });
        return bb.tail("br i1 {}, label {}, label {}", c, t, f);
    } else if (app->callee()->isa<Bot>()) {
        return bb.tail("ret ; bottom: unreachable");
    } else if (auto callee = app->callee()->isa_nom<Lam>(); callee && callee->is_basicblock()) { // ordinary jump
        assert(callee->num_vars() == app->num_args());
        for (size_t i = 0, e = callee->num_vars(); i != e; ++i) {
            if (auto arg = emit_unsafe(app->arg(i)); !arg.empty())
                lam2bb_[callee].phis[callee->var(i)].emplace_back(arg, lam->unique_name());

        }
        bb.tail("br label {}", callee->unique_name());
    } else if (auto callee = app->callee()->isa_nom<Lam>()) { // function/closure call
        auto ret_lam = app->args().back()->as_nom<Lam>();

        std::vector<std::string> args;
        auto app_args = app->args();
        for (auto arg : app_args.skip_back()) {
            if (auto emitted_arg = emit_unsafe(arg); !emitted_arg.empty())
                args.emplace_back(emitted_arg);
        }

        size_t num_vars = ret_lam->num_vars();
        size_t n = 0;
        Array<const Def*> values(num_vars);
        Array<const Def*> types(num_vars);
        for (auto var : ret_lam->vars().skip_back()) {
            if (isa<Tag::Mem>(var->type()) || is_unit(var->type())) continue;
            values[n] = var;
            types[n] = var->type();
            ++n;
        }

        auto name = callee->unique_name();
        auto ret_ty = convert_ret_pi(ret_lam->type());

        bb.tail("{}.ret = {} call({, })", app->unique_name(), ret_ty, args);
        bb.tail("br label {}", ret_lam->unique_name());
        lam2bb_[callee].phis[callee->var(0_s)].emplace_back(app->unique_name(), lam->unique_name());
    }
}

std::string CodeGen::emit_bb(BB& bb, const Def* def) {
    StringStream s;
    auto name = def->unique_name();
    std::string op;

    if (auto lit = def->isa<Lit>()) {
        if (lit->type()->isa<Nat>()) {
            return std::to_string(lit->get<nat_t>());
        } else if (isa<Tag::Int>(lit->type())) {
            auto size = isa_sized_type(lit->type());
            if (size->isa<Top>()) return std::to_string(lit->get<nat_t>());
            if (auto mod = mod2width(as_lit(size))) {
                switch (*mod) {
                    case  1: return std::to_string(lit->get< u1>());
                    case  2:
                    case  4:
                    case  8: return std::to_string(lit->get< u8>());
                    case 16: return std::to_string(lit->get<u16>());
                    case 32: return std::to_string(lit->get<u32>());
                    case 64: return std::to_string(lit->get<u64>());
                    default: THORIN_UNREACHABLE;
                }
            } else {
                return std::to_string(lit->get<u64>());
            }
        }

        if (auto real = isa<Tag::Real>(lit->type())) {
            switch (as_lit<nat_t>(real->arg())) {
                case 16: return std::to_string(lit->get<r16>());
                case 32: return std::to_string(lit->get<r32>());
                case 64: return std::to_string(lit->get<r64>());
                default: THORIN_UNREACHABLE;
            }
        }
        THORIN_UNREACHABLE;
    } else if (def->isa<Bot>()) {
        return "undef";
    } else if (auto bit = isa<Tag::Bit>(def)) {
        auto [a, b] = bit->args<2>([this](auto def) { return emit(def); });
        auto t = convert(bit->type());

        auto neg = [&](const std::string& x) {
            return bb.assign(name + ".neg", "xor {} 0, {}", t, x);
        };

        switch (bit.flags()) {
            case Bit::_and: return bb.assign(name, "and {} {}, {}", t, a, b);
            case Bit:: _or: return bb.assign(name, "or  {} {}, {}", t, a, b);
            case Bit::_xor: return bb.assign(name, "xor {} {}, {}", t, a, b);
            case Bit::nand: return neg(bb.assign(name, "and {} {}, {}", t, a, b));
            case Bit:: nor: return neg(bb.assign(name, "or  {} {}, {}", t, a, b));
            case Bit::nxor: return neg(bb.assign(name, "xor {} {}, {}", t, a, b));
            case Bit:: iff: return bb.assign(name, "and {} {}, {}", neg(a), b);
            case Bit::niff: return bb.assign(name, "or  {} {}, {}", neg(a), b);
            default: THORIN_UNREACHABLE;
        }
    } else if (auto shr = isa<Tag::Shr>(def)) {
        auto [a, b] = shr->args<2>([this](auto def) { return emit(def); });
        auto t = convert(shr->type());

        switch (shr.flags()) {
            case Shr::ashr: op = "ashr"; break;
            case Shr::lshr: op = "lshr"; break;
            default: THORIN_UNREACHABLE;
        }

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto wrap = isa<Tag::Wrap>(def)) {
        auto [a, b] = wrap->args<2>([this](auto def) { return emit(def); });
        auto t = convert(wrap->type());
        auto [mode, width] = wrap->decurry()->args<2>(as_lit<nat_t>);

        switch (wrap.flags()) {
            case Wrap::add: op = "add"; break;
            case Wrap::sub: op = "sub"; break;
            case Wrap::mul: op = "mul"; break;
            case Wrap::shl: op = "shl"; break;
            default: THORIN_UNREACHABLE;
        }

        if (mode & WMode::nuw) op += " nuw";
        if (mode & WMode::nsw) op += " nsw";

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto div = isa<Tag::Div>(def)) {
        auto [m, x, y] = div->args<3>();
        auto t = convert(div->type());
        emit_unsafe(m);
        auto a = emit(x);
        auto b = emit(y);

        switch (div.flags()) {
            case Div::sdiv: op = "sdiv"; break;
            case Div::udiv: op = "udiv"; break;
            case Div::srem: op = "srem"; break;
            case Div::urem: op = "urem"; break;
            default: THORIN_UNREACHABLE;
        }

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto rop = isa<Tag::ROp>(def)) {
        auto [a, b] = rop->args<2>([this](auto def) { return emit(def); });
        auto t = convert(rop->type());
        auto [mode, width] = rop->decurry()->args<2>(as_lit<nat_t>);

        switch (rop.flags()) {
            case ROp::add: op = "fadd"; break;
            case ROp::sub: op = "fsub"; break;
            case ROp::mul: op = "fmul"; break;
            case ROp::div: op = "fdiv"; break;
            case ROp::rem: op = "frem"; break;
            default: THORIN_UNREACHABLE;
        }

        if (mode == RMode::fast)
            op += " fast";
        else {
            if (mode & RMode::nnan    ) op += " nnan";
            if (mode & RMode::ninf    ) op += " ninf";
            if (mode & RMode::nsz     ) op += " nsz";
            if (mode & RMode::arcp    ) op += " arcp";
            if (mode & RMode::contract) op += " contract";
            if (mode & RMode::afn     ) op += " afn";
            if (mode & RMode::reassoc ) op += " reassoc";
        }

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto icmp = isa<Tag::ICmp>(def)) {
        auto [a, b] = icmp->args<2>([this](auto def) { return emit(def); });
        auto t = convert(icmp->arg(0)->type());
        op = "icmp ";

        switch (icmp.flags()) {
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
            default: THORIN_UNREACHABLE;
        }

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto rcmp = isa<Tag::RCmp>(def)) {
        auto [a, b] = rcmp->args<2>([this](auto def) { return emit(def); });
        auto t = convert(rcmp->arg(0)->type());
        op = "fcmp ";

        switch (rcmp.flags()) {
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
            default: THORIN_UNREACHABLE;
        }

        return bb.assign(name, "{} {} {}, {}", op, t, a, b);
    } else if (auto conv = isa<Tag::Conv>(def)) {
        auto src = emit(conv->arg());
        auto src_t = convert(conv->arg()->type());
        auto dst_t = convert(conv->type());

        auto size2width = [&](const Def* type) {
            if (auto int_ = isa<Tag::Int>(type)) {
                if (int_->arg()->isa<Top>()) return 64_u64;
                if (auto width = mod2width(as_lit(int_->arg()))) return *width;
                return 64_u64;
            }
            return as_lit(as<Tag::Real>(type)->arg());
        };

        nat_t s_src = size2width(conv->arg()->type());
        nat_t s_dst = size2width(conv->type());

        switch (conv.flags()) {
            case Conv::s2s: return s_src < s_dst ? op = "sext"  : "trunc";
            case Conv::u2u: return s_src < s_dst ? op = "zext"  : "trunc";
            case Conv::r2r: return s_src < s_dst ? op = "fpext" : "fptrunc";
            case Conv::s2r: return op = "sitofp";
            case Conv::u2r: return op = "uitofp";
            case Conv::r2s: return op = "fptosi";
            case Conv::r2u: return op = "fptoui";
            default: THORIN_UNREACHABLE;
        }

        return bb.assign(name, "{} {} {} to {}", op, src_t, src, dst_t);
    } else if (auto bitcast = isa<Tag::Bitcast>(def)) {
        auto dst_type_ptr = isa<Tag::Ptr>(bitcast->type());
        auto src_type_ptr = isa<Tag::Ptr>(bitcast->arg()->type());
        auto src = emit(bitcast->arg());
        auto src_t = convert(bitcast->arg()->type());
        auto dst_t = convert(bitcast->type());
        if (src_type_ptr && dst_type_ptr) return bb.assign(name,  "bitcast {} {} to {}", src_t, src, dst_t);
        if (src_type_ptr)                 return bb.assign(name, "ptrtoint {} {} to {}", src_t, src, dst_t);
        if (dst_type_ptr)                 return bb.assign(name, "inttoptr {} {} to {}", src_t, src, dst_t);
        return bb.assign(name, "bitcast {} {} to {}", src_t, src, dst_t);
    } else if (auto lea = isa<Tag::LEA>(def)) {
        auto [ptr, index] = lea->args<2>([this](auto def) { return emit(def); });
        return bb.assign(name, "lea {}, {}", ptr, index);
    } else if (auto trait = isa<Tag::Trait>(def)) {
        THORIN_UNREACHABLE;
    } else if (auto alloc = isa<Tag::Alloc>(def)) {
        return bb.assign(name, "TODO");
    } else if (auto slot = isa<Tag::Slot>(def)) {
        return bb.assign(name, "alloca {}");
    } else if (auto load = isa<Tag::Load>(def)) {
        emit_unsafe(load->arg(0));
        auto ptr = emit(load->arg(1));
        return bb.assign(name, "load {}", ptr);
    } else if (auto store = isa<Tag::Store>(def)) {
        emit_unsafe(store->arg(0));
        auto ptr = emit(store->arg(1));
        auto val = emit(store->arg(2));
        return bb.assign(name, "store {}, {}", val, ptr);
    } if (auto tuple = def->isa<Tuple>()) {
        std::string prev = "undef";
        auto t = convert(tuple->type());
        for (size_t i = 0, e = tuple->num_ops(); i != e; ++i) {
            if (auto elem = emit_unsafe(tuple->op(i)); !elem.empty()) {
                auto elem_t = convert(tuple->op(i));
                prev = bb.assign(name + "." + std::to_string(i), "insertvalue {} {}, {} {}, i32 {}", t, prev, elem_t, elem, i);
            }
        }

        return prev;
    } else if (auto pack = def->isa<Pack>()) {
        return "TODO";
    } else if (auto extract = def->isa<Extract>()) {
        auto tuple = emit(extract->tuple());
        auto index = emit(extract->index());
        return bb.assign(name, "extract {}, {}", tuple, index);
    } else if (auto insert = def->isa<Insert>()) {
        auto tuple = emit(insert->tuple());
        auto index = emit(insert->index());
        auto value = emit(insert->value());
        return bb.assign(name, "insert {}, {}, {}", tuple, index, value);
    } else if (auto global = def->isa<Global>()) {
        auto [pointee, addr_space] = as<Tag::Ptr>(global->type())->args<2>();
        vars_decls_.fmt("{} = external global {}\n", name, convert(pointee));
        return name;
    }

    return "<TODO>";
}

std::string CodeGen::emit_fun_decl(Lam* lam) {
    return lam->is_external() ? lam->name() : lam->unique_name();
}

void emit(World& world, Stream& stream) {
    CodeGen cg(world, stream);
    cg.emit_module();
}

}
