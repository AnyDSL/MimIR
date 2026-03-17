#include "mim/plug/core/be/sexpr.h"

#include <iostream>
#include <sstream>

#include <absl/container/btree_set.h>

#include <mim/plug/clos/clos.h>
#include <mim/plug/math/math.h>
#include <mim/plug/mem/mem.h>

#include "mim/def.h"

#include "mim/be/emitter.h"
#include "mim/util/print.h"

using namespace std::string_literals;
using namespace std::literals;

namespace mim::sexpr {

namespace math = mim::plug::math;

namespace {
bool is_const(const Def* def) {
    if (def->isa<Bot>()) return true;
    if (def->isa<Lit>()) return true;
    if (auto pack = def->isa_imm<Pack>()) return is_const(pack->arity()) && is_const(pack->body());

    if (auto tuple = def->isa<Tuple>()) {
        auto ops = tuple->ops();
        return std::ranges::all_of(ops, [](auto def) { return is_const(def); });
    }

    return false;
}

} // namespace

struct BB {
    BB()                    = default;
    BB(const BB&)           = delete;
    BB(BB&& other) noexcept = default;
    BB& operator=(BB other) noexcept { return swap(*this, other), *this; }

    std::deque<std::ostringstream>& head() { return parts[0]; }
    std::deque<std::ostringstream>& body() { return parts[1]; }
    std::deque<std::ostringstream>& tail() { return parts[2]; }

    template<class... Args>
    std::string assign(std::string_view name, const char* s, Args&&... args) {
        print(print(print(body().emplace_back(), "(let {} ", name), s, std::forward<Args&&>(args)...), ")");
        return std::string(name);
    }

    template<class... Args>
    void tail(const char* s, Args&&... args) {
        print(tail().emplace_back(), s, std::forward<Args&&>(args)...);
    }

    friend void swap(BB& a, BB& b) noexcept {
        using std::swap;
        swap(a.phis, b.phis);
        swap(a.parts, b.parts);
    }

    DefMap<std::deque<std::pair<std::string, std::string>>> phis;
    std::array<std::deque<std::ostringstream>, 3> parts;
};

class Emitter : public mim::Emitter<std::string, std::string, BB, Emitter> {
public:
    using Super = mim::Emitter<std::string, std::string, BB, Emitter>;

    Emitter(World& world, std::ostream& ostream)
        : Super(world, "mim_emitter", ostream) {}

    bool is_valid(std::string_view s) { return !s.empty(); }
    void start() override;
    void emit_imported(Lam*);
    void emit_epilogue(Lam*);
    std::string emit_header(Lam*);
    std::string emit_bb(BB&, const Def*);
    std::string emit_curried_app(const App& app);
    std::string prepare();
    void finalize();
    void finalize_nest(const Nest::Node* node, MutSet& done);

private:
    std::string id(const Def*, bool force_bb = false) const;
    std::string convert(const Def*, const Def* = nullptr);

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
    if (def->isa<Axm>()) return def->sym().str();
    if (def->is_external() || (!def->is_set() && def->isa<Lam>())) return def->sym().str();
    if (!def->sym().empty()) return def->sym().str();
    return def->unique_name();
}

std::string Emitter::convert(const Def* type, const Def* var /*= nullptr*/) {
    if (auto i = types_.find(type); i != types_.end()) return i->second;

    std::ostringstream s;
    std::string name;

    if (type->isa<Nat>()) {
        return types_[type] = "nat";
    } else if (auto size = Idx::isa(type)) {
        if (auto lit_size = Idx::size2bitwidth(size)) {
            switch (*lit_size) {
                case 1: return types_[type] = "bool";
                case 8: return types_[type] = "i8";
                case 16: return types_[type] = "i16";
                case 32: return types_[type] = "i32";
                case 64: return types_[type] = "i64";
                default: break;
            }
        }
        print(s, "(idx {})", size);
        return types_[type] = s.str();
    } else if (auto w = math::isa_f(type)) {
        print(s, "(lit {})", type);
        return types_[type] = s.str();
    } else if (auto lit = type->isa<Lit>()) {
        if (lit->type()->isa<Nat>())
            print(s, "(lit {})", lit->get());
        else
            print(s, "(lit {} {})", lit->get(), convert(lit->type()));
    } else if (auto arr = type->isa<Arr>()) {
        if (auto arity = Lit::isa(arr->arity())) {
            u64 size = *arity;
            print(s, "(arr (lit {}) {})", size, convert(arr->body()));
        } else {
            // TODO: Is the arity being emitted correctly?
            print(s, "(arr {} {})", emit_unsafe(arr->arity()), convert(arr->body()));
        }
    } else if (auto pi = type->isa<Pi>()) {
        if (Pi::isa_cn(pi))
            s << "(cn " << convert(pi->dom()) << ")";
        else
            s << "(pi " << convert(pi->dom()) << " " << convert(pi->codom()) << ")";
    } else if (auto sigma = type->isa<Sigma>()) {
        size_t i = 0;
        if (var) {
            assert(var->arity() == sigma->arity());
            print(s, "(sigma { })", Elem(sigma->ops(), [&](auto op) {
                      if (auto v = var->proj(i++))
                          print(s, "(var {} {})", id(v), convert(op, v));
                      else
                          s << op;
                  }));
        } else {
            print(s, "(sigma { })", Elem(sigma->ops(), [&](auto op) { print(s, "{}", convert(op)); }));
        }
    } else if (auto tuple = type->isa<Tuple>()) {
        print(s, "(tuple { })", Elem(tuple->ops(), [&](auto op) { print(s, "{}", convert(op)); }));
    } else if (auto app = type->isa<App>()) {
        print(s, "(app {} {})", convert(app->callee()), convert(app->arg()));
    } else if (auto ax = type->isa<Axm>()) {
        print(s, "{}", ax->sym().str());
    } else if (auto hole = type->isa<Hole>()) {
        print(s, "(hole {})", id(hole));
    } else if (auto extract = type->isa<Extract>()) {
        print(s, "(extract {} {})", convert(extract->tuple()), convert(extract->index()));
    } else if (auto mType = type->isa<Type>()) {
        if (auto level = Lit::isa(mType->level())) {
            if (level == 0) print(s, "(type (lit 0))");
            if (level == 1) print(s, "(type (lit 1))");
        }
        print(s, "(type {})", convert(mType->level()));
    } else if (type->isa<Univ>()) {
        print(s, "univ");
    } else {
        error("unsupported type '{}'", type);
        fe::unreachable();
    }

    if (name.empty()) return types_[type] = s.str();

    assert(!s.str().empty());
    return types_[type] = name;
}

/*
 * emit
 */

void Emitter::start() {
    Super::start();

    // TODO: do we need this?
    // for (auto import : world().driver().imports().syms())
    //     print(ostream(), "{} {};\n", world().driver().is_loaded(import) ? "plugin" : "import", import);

    for (auto&& decl : decls_)
        ostream() << decl << '\n';
    ostream() << func_decls_.str() << '\n';
    ostream() << vars_decls_.str() << '\n';
    ostream() << func_impls_.str() << '\n';
}

void Emitter::emit_imported(Lam* lam) {
    print(func_decls_, "cfun {}(", id(lam));

    auto doms = lam->doms();
    for (auto sep = ""; auto dom : doms.view()) {
        print(func_decls_, "{}{}", sep, convert(dom));
        sep = ", ";
    }

    print(func_decls_, ")\n");
}

std::string Emitter::emit_header(Lam* lam) {
    std::ostringstream os;

    const std::string lam_kind = lam->isa_cn(lam) ? "con" : "lam";
    const std::string ext      = lam->is_external() ? "extern" : "intern";
    tab.println(os, "({} {} {}", lam_kind, ext, id(lam));

    ++tab;
    tab.println(os, "(tuple");
    if (lam->has_var()) {
        ++tab;
        auto vars = lam->vars();
        for (int i = 0; auto var : vars.view()) {
            if (var) {
                tab.println(os, "(var {}", id(var));
                ++tab;
                tab.println(os, "{}", convert(var->type(), var));
                --tab;
                tab.println(os, ")");
            } else {
                // TODO: anonymous var construction (egg expects (var <name> <type>))
                tab.println(os, "(var");
                ++tab;
                tab.println(os, "{}", convert(lam->dom(i)));
                --tab;
                tab.println(os, ")");
            }
            ++i;
        }
        --tab;
    }
    tab.print(os, ")");
    --tab;

    tab.print(os, "{}", emit_unsafe(lam->filter()));

    return os.str();
}

// TODO: emit_unsafe() for Defs that were already
// emitted uses caching which messes up indentation.
// If we don't need basic blocks just use emit_bb with
// some dummy basic blocks instead.
std::string Emitter::emit_curried_app(const App& app) {
    std::ostringstream os;
    ++tab;
    tab.lnprint(os, "(app ");

    if (auto app_callee = app.callee()->isa<App>())
        tab.print(os, "{}", emit_curried_app(*app_callee));
    else
        tab.print(os, "{}", emit_unsafe(app.callee()));
    tab.print(os, "{}", emit_unsafe(app.arg()));

    tab.lnprint(os, ")");
    --tab;
    return os.str();
}

std::string Emitter::prepare() { return root()->unique_name(); }

void Emitter::finalize_nest(const Nest::Node* node, MutSet& done) {
    if (!node->mut()->isa<Lam>()) return;
    if (auto [_, ins] = done.emplace(node->mut()); !ins) return;

    auto lam = node->mut()->as_mut<Lam>();
    assert(lam2bb_.contains(lam));

    print(func_impls_, "{}", emit_header(lam));

    if (lam->isa_cn(lam)) {
        auto app = lam->body()->as<App>();
        print(func_impls_, "{}", emit_curried_app(*app));
    } else {
        print(func_impls_, "{}", emit(lam->body()));
    }

    print(func_impls_, "\n)\n\n");
}

void Emitter::finalize() {
    if (root()->sym().str() == "_fallback_compile") return;
    MutSet done;
    finalize_nest(nest().root(), done);
}

void Emitter::emit_epilogue(Lam* lam) { return; }

std::string Emitter::emit_bb(BB& bb, const Def* def) {
    std::ostringstream os;

    if (def->type()->isa<Type>() || def->type()->isa<Univ>()) {
        ++tab;
        tab.lnprint(os, convert(def).c_str());
        --tab;

        return os.str();
    } else if (auto lam = def->isa<Lam>()) {
        print(os, "\n");
        ++tab;
        print(os, emit_header(lam->as_mut<Lam>()).c_str());
        if (lam->isa_cn(lam)) {
            auto app = lam->body()->as<App>();
            print(os, emit_curried_app(*app).c_str());
        } else {
            print(os, emit(lam->body()).c_str());
        }
        tab.lnprint(os, ")");
        --tab;

        return os.str();
    } else if (auto lit = def->isa<Lit>()) {
        ++tab;
        if (lit->type()->isa<Nat>())
            tab.lnprint(os, "(lit {})", lit->get<u64>());
        else if (auto size = Idx::isa(lit->type()))
            if (auto lit_size = Idx::size2bitwidth(size); lit_size && *lit_size == 1)
                tab.lnprint(os, "(lit {})", lit);
            else
                tab.lnprint(os, "(lit {} {})", lit->get(), convert(lit->type()));
        else
            tab.lnprint(os, "(lit {})", lit);
        --tab;

        return os.str();
    } else if (auto tuple = def->isa<Tuple>()) {
        ++tab;
        tab.lnprint(os, "(tuple");
        for (auto sep = ""; auto e : tuple->ops()) {
            if (auto v_elem = emit_unsafe(e); !v_elem.empty()) {
                os << sep << v_elem;
                sep = " ";
            }
        }
        tab.lnprint(os, ")");
        --tab;

        return os.str();
    } else if (auto seq = def->isa<Seq>()) {
        // NOTE: are there any cases where this would not just be an (arr) node?
        auto shape = seq->arity();
        auto body  = seq->body();

        ++tab;
        tab.lnprint(os, "(arr");
        tab.print(os, emit_bb(bb, shape).c_str());
        tab.print(os, emit_bb(bb, body).c_str());
        tab.lnprint(os, ")");
        --tab;

        return os.str();
    } else if (auto extract = def->isa<Extract>()) {
        auto tuple = extract->tuple();
        auto index = extract->index();

        if (auto lit = Lit::isa(index); lit && tuple->isa<Var>()) {
            ++tab;
            tab.lnprint(os, id(extract).c_str());
            --tab;
            return os.str();
        }

        ++tab;
        tab.lnprint(os, "(extract");
        tab.print(os, emit_bb(bb, tuple).c_str());
        tab.print(os, emit_bb(bb, index).c_str());
        tab.lnprint(os, ")");
        --tab;

        return os.str();
    } else if (auto insert = def->isa<Insert>()) {
        auto tuple = insert->tuple();
        auto index = insert->index();
        auto value = insert->value();

        ++tab;
        tab.lnprint(os, "(ins");
        tab.print(os, emit_bb(bb, tuple).c_str());
        tab.print(os, emit_bb(bb, index).c_str());
        tab.print(os, emit_bb(bb, value).c_str());
        tab.lnprint(os, ")");
        --tab;

        return os.str();
    } else if (auto var = def->isa<Var>()) {
        ++tab;
        tab.lnprint(os, id(var).c_str());
        --tab;

        return os.str();
    } else if (auto app = def->isa<App>()) {
        print(os, emit_curried_app(*app).c_str());

        return os.str();
    } else if (auto axm = def->isa<Axm>()) {
        ++tab;
        tab.lnprint(os, id(axm).c_str());
        --tab;

        return os.str();
        // TODO: emit sexpr
    } else if (def->isa<Top>() || def->isa<Bot>()) {
        std::string symbol = def->isa<Top>() ? "T" : "⊥";
        if (def->sym().empty())
            print(os, "{}:{}", symbol, convert(def->type()));
        else
            return bb.assign(id(def), "{}:{}", symbol, convert(def->type()));
    } else {
        error("unhandled def in Mim backend: {} : {}", def, def->type());
    }

    return os.str();
}

void emit(World& world, std::ostream& ostream) {
    Emitter emitter(world, ostream);
    emitter.run();
}

} // namespace mim::sexpr
