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

struct BB {
    BB()                    = default;
    BB(const BB&)           = delete;
    BB(BB&& other) noexcept = default;
    BB& operator=(BB other) noexcept { return swap(*this, other), *this; }

    std::deque<std::ostringstream>& head() { return parts[0]; }
    std::deque<std::ostringstream>& body() { return parts[1]; }
    std::deque<std::ostringstream>& tail() { return parts[2]; }

    template<class... Args>
    void body(const char* s, Args&&... args) {
        print(body().emplace_back(), s, std::forward<Args&&>(args)...);
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
        : Super(world, "sexpr_emitter", ostream) {}

    bool is_valid(std::string_view s) { return !s.empty(); }
    void start() override;
    void emit_imported(Lam*);
    void emit_epilogue(Lam*);
    std::string emit_header(Lam*, bool as_binding = false);
    std::string emit_bb(BB&, const Def*);
    std::string emit_curried_app(const App& app);
    std::string prepare();
    void finalize();
    void finalize_nest(const Nest::Node* node, MutSet& done);

private:
    std::string id(const Def*) const;
    std::string indent_lines(std::string s, unsigned tabs);
    std::string convert(const Def*, const Def* = nullptr);

    std::ostringstream func_decls_;
    std::ostringstream func_impls_;
};

std::string Emitter::id(const Def* def) const {
    if (def->isa<Axm>()) return def->sym().str();
    return def->unique_name();
}

std::string Emitter::indent_lines(std::string s, unsigned tabs) {
    std::stringstream ss(s);
    std::string indent(tabs * 4, ' ');
    std::string line;
    std::string result;
    bool first = true;

    while (std::getline(ss, line)) {
        if (!first) result += "\n";
        result += indent + line;
        first = false;
    }

    return result;
}

std::string Emitter::convert(const Def* type, const Def* var /*= nullptr*/) {
    if (auto i = types_.find(type); i != types_.end()) return i->second;

    std::ostringstream s;
    if (type->isa<Bot>()) {
        std::string symbol = "⊥";
        if (type->sym().empty())
            print(s, "(bot {} {})", symbol, convert(type->type()));
        else
            print(s, "(bot {} {})", id(type), convert(type->type()));
    } else if (type->isa<Top>()) {
        std::string symbol = "T";
        if (type->sym().empty())
            print(s, "(top {} {})", symbol, convert(type->type()));
        else
            print(s, "(top {} {})", id(type), convert(type->type()));
    } else if (type->isa<Nat>()) {
        print(s, "Nat");
    } else if (auto size = Idx::isa(type)) {
        if (auto lit_size = Idx::size2bitwidth(size)) {
            switch (*lit_size) {
                case 1: return types_[type] = "Bool";
                case 8: return types_[type] = "I8";
                case 16: return types_[type] = "I16";
                case 32: return types_[type] = "I32";
                case 64: return types_[type] = "I64";
                default: break;
            }
        }
        print(s, "(idx (lit {}))", size);
    } else if (auto w = mim::plug::math::isa_f(type)) {
        print(s, "(lit {})", type);
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
            // TODO: Is there a case where we would need emit_bb for arr->arity()?
            print(s, "(arr {} {})", convert(arr->arity()), convert(arr->body()));
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

    return types_[type] = s.str();
}

void Emitter::start() {
    Super::start();

    ostream() << func_decls_.str() << '\n';
    ostream() << func_impls_.str() << '\n';

    // TODO: use pretty(sexpr, line_len) from the egg FFI
    // to pretty-print the sexpr either based on a switch from the cli
    // or as default after the sexpr backend has been completed
}

// We assume that the lambda will be a continuation since imports
// only exist via cfun and ccon which are both internally modelled as con
void Emitter::emit_imported(Lam* lam) {
    const std::string ext = lam->is_external() ? "extern" : "intern";

    print(func_decls_, "(con {} {} (sigma ", ext, id(lam));

    auto doms = lam->doms();
    for (auto sep = ""; auto dom : doms.view()) {
        print(func_decls_, "{}{}", sep, convert(dom));
        sep = " ";
    }

    print(func_decls_, "))\n");
}

// TODO: should we emit the codomain seperately for lam?
std::string Emitter::emit_header(Lam* lam, bool as_binding) {
    std::ostringstream os;

    const std::string lam_kind = lam->isa_cn(lam) ? "con" : "lam";
    const std::string ext      = lam->is_external() ? "extern" : "intern";

    if (as_binding)
        tab.println(os, "(let {} ({} {} {}", id(lam), lam_kind, ext, id(lam));
    else
        tab.println(os, "({} {} {}", lam_kind, ext, id(lam));

    ++tab;
    tab.println(os, "(sigma");
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
                tab.println(os, "{}", convert(lam->dom(i)));
            }
            ++i;
        }
        --tab;
    }
    tab.print(os, ")");
    --tab;

    auto dummy = BB{};
    tab.print(os, "{}", emit_bb(dummy, lam->filter()));

    return os.str();
}

// TODO: emit_unsafe() for Defs that were already
// emitted uses caching which messes up indentation.
// If we don't need basic blocks just use emit_bb with
// some dummy basic blocks instead.
// - rewrite the whole dummy BB thing
std::string Emitter::emit_curried_app(const App& app) {
    std::ostringstream os;
    ++tab;
    tab.lnprint(os, "(app ");

    if (auto app_callee = app.callee()->isa<App>())
        tab.print(os, "{}", emit_curried_app(*app_callee));
    else {
        auto dummy = BB{};
        tab.print(os, "{}", emit_bb(dummy, app.callee()));
    }
    auto dummy = BB{};
    tab.print(os, "{}", emit_bb(dummy, app.arg()));

    tab.lnprint(os, ")");
    --tab;
    return os.str();
}

std::string Emitter::prepare() { return root()->unique_name(); }

void Emitter::finalize_nest(const Nest::Node* node, MutSet& done) {
    done.emplace(node->mut());
    auto lam = node->mut()->as_mut<Lam>();
    assert(lam2bb_.contains(lam));
    auto& bb = lam2bb_[lam];

    // Prints the declaration or header (name dom->codom etc.) of the lambda (as part of a let binding for internal
    // lambdas)
    if (lam->is_closed())
        print(func_impls_, "{}", emit_header(lam));
    else {
        ++tab;
        print(func_impls_, "\n{}", emit_header(lam, true));
    }

    // Would print temporary variable let bindings if we were to use those (would have to print let bindings to body()
    // in emit_bb())
    for (auto& line : bb.body())
        print(func_impls_, "{}", line.str());

    // Prints lambda let bindings based on nesting structure
    // while keeping count of the number of lambda bindings the current lambdas' body depends on
    auto lam_binding_count = 0;
    for (auto op : node->mut()->deps()) {
        for (auto mut : op->local_muts())
            if (auto next = nest()[mut]) {
                if (next->mut()->isa<Lam>() && !done.contains(next->mut())) {
                    finalize_nest(next, done);
                    lam_binding_count++;
                }
            }
    }

    // Prints the actual body of the lambda after dependent bindings have been printed (as defined by prints to tail()
    // in emit_bb())
    for (auto& line : bb.tail()) {
        auto indented = indent_lines(line.str(), tab.indent());
        print(func_impls_, "{}", indented);
    }

    // Closes every lambda let binding for the current lambdas' body
    std::string close_bindings(lam_binding_count, ')');
    print(func_impls_, "{}", close_bindings);

    // Closes the current lambda
    if (lam->is_closed()) {
        tab.lnprint(func_impls_, ")\n\n");
    } else {
        tab.lnprint(func_impls_, ")");
        --tab;
    }
}

// TODO: if a lambda depends on another closed internal lambda, then this
// internal lambda should be printed before the current lambda to keep the order
// of dependency/conversion order in the egg rewrite phase correct. (as in pow.mim)
// - one call to finalize_nest to emit all internal, closed lambdas followed by another one
//   emitting all external, closed lambdas?
// - maybe we can also just loop through the lambdas as in ll.cpp and once we find a lam
//   that is closed and internal, we just emit it as above
void Emitter::finalize() {
    if (root()->sym().str() == "_fallback_compile") return;
    MutSet done;
    finalize_nest(nest().root(), done);
}

void Emitter::emit_epilogue(Lam* lam) {
    auto& bb = lam2bb_[lam];

    if (lam->isa_cn(lam)) {
        auto app = lam->body()->as<App>();
        bb.tail("{}", emit_curried_app(*app));
    } else {
        bb.tail("{}", emit(lam->body()));
    }
}

std::string Emitter::emit_bb(BB& bb, const Def* def) {
    std::ostringstream os;

    ++tab;
    if (def->type()->isa<Type>() || def->type()->isa<Univ>()) {
        tab.lnprint(os, convert(def).c_str());
    } else if (auto lam = def->isa<Lam>()) {
        tab.lnprint(os, id(lam).c_str());
    } else if (auto lit = def->isa<Lit>()) {
        if (lit->type()->isa<Nat>())
            // TODO: consider printing term aliases like "i32" for 2^32
            // but that would require upper case printing of types like "I32" to prevent confusion
            tab.lnprint(os, "(lit {})", lit->get<u64>());
        else if (auto size = Idx::isa(lit->type()))
            if (auto lit_size = Idx::size2bitwidth(size); lit_size && *lit_size == 1)
                tab.lnprint(os, "(lit {})", lit);
            else
                tab.lnprint(os, "(lit {} {})", lit->get(), convert(lit->type()));
        else
            tab.lnprint(os, "(lit {})", lit);
    } else if (auto tuple = def->isa<Tuple>()) {
        tab.lnprint(os, "(tuple");
        for (auto sep = ""; auto e : tuple->ops()) {
            if (auto v_elem = emit_bb(bb, e); !v_elem.empty()) {
                os << sep << v_elem;
                sep = " ";
            }
        }
        tab.lnprint(os, ")");
    } else if (auto seq = def->isa<Seq>()) {
        auto shape = seq->arity();
        auto body  = seq->body();

        tab.lnprint(os, "(pack");
        tab.print(os, emit_bb(bb, shape).c_str());
        tab.print(os, emit_bb(bb, body).c_str());
        tab.lnprint(os, ")");
    } else if (auto extract = def->isa<Extract>()) {
        auto tuple = extract->tuple();
        auto index = extract->index();

        // TODO: if we have an extract from a sigma- or arr-typed variable i.e.
        // a projection, it would be better just to print the identifier of the projection
        // - for example in loop.mim we are printing extracts instead of argc and mem
        if (auto lit = Lit::isa(index); lit && tuple->isa<Var>()) {
            tab.lnprint(os, id(extract).c_str());
        } else {
            tab.lnprint(os, "(extract");
            tab.print(os, emit_bb(bb, tuple).c_str());
            tab.print(os, emit_bb(bb, index).c_str());
            tab.lnprint(os, ")");
        }
    } else if (auto insert = def->isa<Insert>()) {
        auto tuple = insert->tuple();
        auto index = insert->index();
        auto value = insert->value();

        tab.lnprint(os, "(ins");
        tab.print(os, emit_bb(bb, tuple).c_str());
        tab.print(os, emit_bb(bb, index).c_str());
        tab.print(os, emit_bb(bb, value).c_str());
        tab.lnprint(os, ")");
    } else if (auto var = def->isa<Var>()) {
        tab.lnprint(os, id(var).c_str());
    } else if (auto app = def->isa<App>()) {
        --tab;
        print(os, emit_curried_app(*app).c_str());
        ++tab;
    } else if (auto axm = def->isa<Axm>()) {
        tab.lnprint(os, id(axm).c_str());
    } else if (def->isa<Bot>()) {
        std::string symbol = "⊥";
        if (def->sym().empty())
            tab.lnprint(os, "(bot {} {})", symbol, convert(def->type()));
        else
            tab.lnprint(os, "(bot {} {})", id(def), convert(def->type()));
    } else if (def->isa<Top>()) {
        std::string symbol = "T";
        if (def->sym().empty())
            tab.lnprint(os, "(top {} {})", symbol, convert(def->type()));
        else
            tab.lnprint(os, "(top {} {})", id(def), convert(def->type()));
    } else {
        error("unhandled def in Mim backend: {} : {}", def, def->type());
    }
    --tab;

    return os.str();
}

void emit(World& world, std::ostream& ostream) {
    Emitter emitter(world, ostream);
    emitter.run();
}

} // namespace mim::sexpr
