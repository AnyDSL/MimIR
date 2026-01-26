#include "mim/plug/core/be/sexpr.h"

#include <iostream>
#include <sstream>

#include <absl/container/btree_set.h>

#include <mim/plug/clos/clos.h>
#include <mim/plug/math/math.h>
#include <mim/plug/mem/mem.h>

#include "mim/def.h"
#include "mim/driver.h"

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

std::string_view external(const Def* def) {
    if (def->is_external()) return "extern "sv;
    return ""sv;
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
    std::string emit_con(Lam*);
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

    // assert(!Axm::isa<mem::M>(type));
    std::ostringstream s;
    std::string name;

    if (type->isa<Nat>()) {
        return types_[type] = "Nat";
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
        print(s, "(Idx {})", size);
        return types_[type] = s.str();
    } else if (auto w = math::isa_f(type)) {
        print(s, "{}", type);
        return types_[type] = s.str();
    } else if (auto lit = type->isa<Lit>()) {
        if (lit->type()->isa<Nat>())
            print(s, "{}", lit->get());
        else
            print(s, "{}:{}", lit->get(), convert(lit->type()));
    } else if (auto arr = type->isa<Arr>()) {
        auto t_elem = convert(arr->body());
        if (auto arity = Lit::isa(arr->arity())) {
            u64 size = *arity;
            print(s, "<<{};{}>>", size, t_elem);
        } else {
            print(s, "<<{};{}>>", emit_unsafe(arr->arity()), t_elem);
        }
    } else if (auto pi = type->isa<Pi>()) {
        if (Pi::isa_cn(pi))
            s << "Cn " << convert(pi->dom());
        else
            s << "Pi " << convert(pi->dom()) << " -> " << convert(pi->codom());
    } else if (auto sigma = type->isa<Sigma>()) {
        size_t i = 0;
        if (var) {
            assert(var->arity() == sigma->arity());
            print(s, "[{,}]", Elem(sigma->ops(), [&](auto op) {
                      if (auto v = var->proj(i++))
                          print(s, "{}:{}", id(v), convert(op, v));
                      else
                          s << op;
                  }));
        } else {
            print(s, "[{,}]", sigma->ops());
        }
    } else if (auto tuple = type->isa<Tuple>()) {
        print(s, "({,})", Elem(tuple->ops(), [&](auto op) { print(s, "{}", convert(op)); }));
    } else if (auto app = type->isa<App>()) {
        print(s, "{} {}", convert(app->callee()), convert(app->arg()));
    } else if (auto ax = type->isa<Axm>()) {
        print(s, "{}", ax->sym().str());
    } else if (auto hole = type->isa<Hole>()) {
        print(s, "{}", id(hole));
    } else if (auto extract = type->isa<Extract>()) {
        print(s, "{}", extract);
    } else if (auto mType = type->isa<Type>()) {
        if (auto level = Lit::isa(mType->level())) {
            if (level == 0) print(s, "★");
            if (level == 1) print(s, "□");
        }
        print(s, "(Type {})", convert(mType->level()));
    } else if (type->isa<Univ>()) {
        print(s, "Univ");
    } else {
        error("unsupported type '{}'", type);
        fe::unreachable();
    }

    if (name.empty()) return types_[type] = s.str();

    assert(!s.str().empty());
    // type_decls_ << s.str() << '\n';
    return types_[type] = name;
}

/*
 * emit
 */

void Emitter::start() {
    // Calls the run() method of NestPhase that computes a Nest for each closed mutable in the world
    // and then calls the visit() method defined inside of the generic Emitter class for each of them.
    // This visit method is responsible for corresponding each lambda with a basic block and filling these
    // basic blocks (the tail, to be precise) with a string representation of the lambda. These basic block
    // representations will then be written into func_impls_ via the finalize() method called by visit() and
    // then printed out in the following.
    Super::start();

    for (auto import : world().driver().imports().syms())
        print(ostream(), "{} {};\n", world().driver().is_loaded(import) ? "plugin" : "import", import);

    // NOTE: seems to be unused (never written to)
    for (auto&& decl : decls_)
        ostream() << decl << '\n';
    // written to in emit_imported (external import functions)
    ostream() << func_decls_.str() << '\n';
    // NOTE: seems to be unused (never written to)
    ostream() << vars_decls_.str() << '\n';
    // written to in finalize_nest and emit_con (called by finalize_nest)
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

std::string Emitter::emit_con(Lam* lam) {
    print(std::cout, "emit_con: {}\n", lam->unique_name());

    std::ostringstream os;

    const std::string lam_kind = lam->isa_cn(lam) ? "con" : "lam";
    // tab.print(func_impls_, "{} {}{} [", lam_kind, external(lam), id(lam));

    // TODO: maybe extern needs to be emitted aswell for reconstruction
    // tab.print(func_impls_, "({} {}{} ", lam_kind, external(lam), id(lam));
    print(os, "({} {} (", lam_kind, id(lam));

    if (lam->has_var()) {
        auto vars  = lam->vars();
        unsigned i = 0;
        for (auto sep = ""; auto var : vars.view()) {
            if (var) {
                auto name = id(var);
                print(os, "{}{}:{}", sep, name, convert(var->type(), var));
            } else {
                print(os, "{}{}", sep, convert(lam->dom(i)));
            }
            sep = " ";
            ++i;
        }
    }
    // print(func_impls_, "]@({}) = \n", emit_unsafe(lam->filter()));
    print(os, ")");
    return os.str();
}

// NOTE: when proecessing a continuation, this method is called to emit the tail of its basic block in emit_epilogue
// and this then further calls on emit_unsafe() for the callee and arg in order to emit the body of the basic block in
// emit_bb()
std::string Emitter::emit_curried_app(const App& app) {
    std::ostringstream os;
    auto v_arg = emit_unsafe(app.arg());
    if (auto app_callee = app.callee()->isa<App>()) {
        auto v_callee = emit_curried_app(*app_callee);
        print(os, "(app {} {})", v_callee, v_arg);
    } else {
        auto v_callee = emit_unsafe(app.callee());
        print(os, "(app {} {})", v_callee, v_arg);
    }
    return os.str();
}

std::string Emitter::prepare() { return root()->unique_name(); }

void Emitter::finalize_nest(const Nest::Node* node, MutSet& done) {
    if (!node->mut()->isa<Lam>()) return;
    if (auto [_, ins] = done.emplace(node->mut()); !ins) return;

    auto lam = node->mut()->as_mut<Lam>();
    assert(lam2bb_.contains(lam));
    auto& bb = lam2bb_[lam];
    print(func_impls_, "{} ", emit_con(lam));

    // ++tab;
    // first prints all lines of the head and body of the basic block into func_impls_
    for (const auto& part : bb.parts | std::views::take(2))
        for (auto& line : part)
            print(func_impls_, "{}", line.str());

    // for (auto op : node->mut()->deps()) {
    //     for (auto mut : op->local_muts())
    //         if (auto next = nest()[mut]) {
    //             // recursively calls finalize_nest() on nested lambdas
    //             // as part of the mutables that the lambda depends on (node->mut()->deps())
    //             // for non-lambda muts finalize_nest() will simply return
    //             finalize_nest(next, done);
    //         }
    // }

    for (const auto& line : bb.tail())
        tab.print(func_impls_, "{}", line.str());
    // --tab;
    print(func_impls_, ")\n");
    // func_impls_ << std::endl;
}

void Emitter::finalize() {
    if (root()->sym().str() == "_fallback_compile") {
        // This is a fallback compile function, we don't want to emit it
        return;
    }
    MutSet done;
    finalize_nest(nest().root(), done);
}

// NOTE: we basically have lambdas whose structure is as follows:
// 1) The signature of the lambda like:  lam extern foo(Nat): Nat
// 2) The body, consisting of let assignments like:
//        let i = bar#1:(Idx 3)
//    and other nested lambdas like:
//        con baz(Nat): Nat ...
// 3) The tail, which is emitted below, where the actual logic of the lambda happens like:  baz i
// This method will be called for every lambda inside of the loop in the emitter's visit() method.
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
    // if (auto lam = def->isa<Lam>()) return id(lam);
    if (def->type()->isa<Type>() || def->type()->isa<Univ>()) {
        // This is a type, we don't emit it as an instruction
        return convert(def);
    }

    // print(std::cout, "debug {}\n", def);

    std::ostringstream os;
    if (auto lam = def->isa<Lam>()) {
        os << emit_con(lam->as_mut<Lam>()) << " ";
        if (lam->isa_cn(lam)) {
            auto app = lam->body()->as<App>();
            os << emit_curried_app(*app);
        } else {
            os << emit(lam->body());
        }
        return os.str();
    } else if (auto lit = def->isa<Lit>()) {
        if (lit->type()->isa<Nat>())
            print(os, "{}", lit->get<u64>());
        else if (auto size = Idx::isa(lit->type()))
            if (auto lit_size = Idx::size2bitwidth(size); lit_size && *lit_size == 1)
                print(os, "{}", lit);
            else
                print(os, "{}:{}", lit->get(), convert(lit->type()));
        else
            print(os, "{}", lit);
        return os.str();
    } else if (def->type()->isa<Nat>()) {
        print(os, "{}", def->unique_name());
        return os.str();
    } else if (auto tuple = def->isa<Tuple>()) {
        os << "(tuple ";
        for (auto sep = ""; auto e : tuple->ops()) {
            if (auto v_elem = emit_unsafe(e); !v_elem.empty()) {
                os << sep << v_elem;
                sep = " ";
            }
        }
        os << ")";
        return os.str();
    } else if (auto seq = def->isa<Seq>()) {
        auto body  = emit_unsafe(seq->body());
        auto shape = emit_unsafe(seq->arity());

        return bb.assign(id(seq), "<<{}; {}>>", shape, body);
    } else if (auto extract = def->isa<Extract>()) {
        auto tuple = extract->tuple();
        auto index = extract->index();

        // need to emit the tuple even if we're just using the id!
        auto tuple_str = emit_unsafe(tuple);
        if (auto lit = Lit::isa(index); lit && tuple->isa<Var>()) return id(extract);

        // return bb.assign(id(extract), "(extract {} {})", tuple_str, emit_unsafe(index));
        os << "(extract " << tuple_str << " " << emit_unsafe(index) << ")";
        return os.str();
    } else if (auto insert = def->isa<Insert>()) {
        auto tuple = insert->tuple();
        auto index = insert->index();
        auto value = insert->value();

        // return bb.assign(id(insert), "(ins {} {} {})", emit_unsafe(tuple), emit_unsafe(index), emit_unsafe(value));
        os << "(ins " << emit_unsafe(tuple) << " " << emit_unsafe(index) << " " << emit_unsafe(value) << ")";
        return os.str();
    } else if (auto var = def->isa<Var>()) {
        return id(var);
    } else if (auto app = def->isa<App>()) {
        return bb.assign(id(app), "{}", emit_curried_app(*app));
    } else if (auto axm = def->isa<Axm>()) {
        return id(axm);
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
