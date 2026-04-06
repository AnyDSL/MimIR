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

    template<class... Args>
    std::string assign(Tab tab, std::string name, const char* s, Args&&... args) {
        if (!assigned.contains(name)) {
            assigned.insert(name);
            auto& os = body().emplace_back();
            print(tab.lnprint(os, "(let {} ", name), s, std::forward<Args&&>(args)...);
        }
        return name;
    }

    template<class Fn>
    std::string assign(Tab tab, std::string name, Fn&& print_term) {
        if (!assigned.contains(name)) {
            assigned.insert(name);
            auto& os = body().emplace_back();
            tab.lnprint(os, "(let {} ", name);
            print_term(os);
        }
        return name;
    }

    friend void swap(BB& a, BB& b) noexcept {
        using std::swap;
        swap(a.phis, b.phis);
        swap(a.parts, b.parts);
    }

    DefMap<std::deque<std::pair<std::string, std::string>>> phis;
    std::array<std::deque<std::ostringstream>, 3> parts;
    std::set<std::string> assigned;
};

class Emitter : public mim::Emitter<std::string, std::string, BB, Emitter> {
public:
    using Super = mim::Emitter<std::string, std::string, BB, Emitter>;

    Emitter(World& world, std::ostream& ostream)
        : Super(world, "sexpr_emitter", ostream) {}

    bool is_valid(std::string_view s) { return !s.empty(); }
    void start() override;
    void emit_imported(Lam*);
    std::string prepare();
    void emit_epilogue(Lam*);
    void finalize();
    std::string emit_header(Lam*, bool as_binding = false);
    void emit_lam(Lam* lam, MutSet& done);
    std::string emit_curried_app(BB&, const App& app);
    std::string emit_bb(BB&, const Def*);

private:
    std::string id(const Def*) const;
    std::string indented(size_t tabs, std::string s);
    std::string convert(const Def*, const Def* = nullptr);

    std::ostringstream func_decls_;
    std::ostringstream func_impls_;
    std::ostringstream rewrite_rules_;
};

// Axioms, declarations(imports) and externals need to be emitted without a uid
std::string Emitter::id(const Def* def) const {
    if (def->isa<Axm>())
        return def->sym().str();
    else if (def->isa<Lam>() && !def->is_set())
        return def->sym().str();
    else if (def->is_external())
        return def->sym().str();

    return def->unique_name();
}

// Adjusts the base indentation of a string like
// "        (app
//              foo
//              bar
//          )"
// to the number of tabs specified with 'tabs' (i.e. for tabs=1)
// "    (app
//          foo
//          bar
//      )"
std::string Emitter::indented(size_t tabs, std::string s) {
    while (!s.empty() && (s.front() == '\n' || s.front() == '\r'))
        s.erase(0, 1);

    std::stringstream ss(s);
    std::string indent(tabs * 4, ' ');
    std::string line;
    std::string result;

    size_t min_indent = s.find_first_not_of(' ');
    while (std::getline(ss, line)) {
        // Skips empty lines
        if (line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
        result += "\n" + indent + line.substr(min_indent);
    }

    return result;
}

std::string Emitter::convert(const Def* type, const Def* var /*= nullptr*/) {
    if (auto i = types_.find(type); i != types_.end()) return i->second;

    std::ostringstream s;
    if (type->isa<Bot>()) {
        print(s, "(bot {})", convert(type->type()));
    } else if (type->isa<Top>()) {
        print(s, "(top {})", convert(type->type()));
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
        print(s, "{}", id(ax));
    } else if (auto hole = type->isa<Hole>()) {
        print(s, "(hole {})", id(hole));
    } else if (auto extract = type->isa<Extract>()) {
        print(s, "(extract {} {})", convert(extract->tuple()), convert(extract->index()));
    } else if (auto mType = type->isa<Type>()) {
        if (auto level = Lit::isa(mType->level())) {
            if (level == 0) print(s, "(type (lit 0))");
            if (level == 1) print(s, "(type (lit 1))");
        } else {
            print(s, "(type {})", convert(mType->level()));
        }
    } else if (type->isa<Univ>()) {
        print(s, "Univ");
    } else if (auto reform = type->isa<Reform>()) {
        print(s, "(reform {})", convert(reform->meta_type()));
    } else {
        error("unsupported type '{}'", type);
        fe::unreachable();
    }

    return types_[type] = s.str();
}

void Emitter::start() {
    Super::start();

    ostream() << rewrite_rules_.str();
    ostream() << func_decls_.str();
    ostream() << func_impls_.str();

    // TODO: Use pretty(sexpr, line_len) from the egg FFI
    // to pretty-print the sexpr either based on a switch from the cli
    // or as default after the sexpr backend has been completed
}

// We assume that the lambda will be a continuation since imports
// only exist via cfun and ccon which are both internally modelled as con
void Emitter::emit_imported(Lam* lam) {
    const std::string ext = lam->is_external() ? "extern" : "intern";

    print(func_decls_, "(con {} {}", ext, id(lam));

    ++tab;
    tab.lnprint(func_decls_, "(sigma");
    ++tab;
    auto doms = lam->doms();
    for (auto dom : doms.view())
        tab.lnprint(func_decls_, "{}", convert(dom));
    print(func_decls_, "))\n\n");
    --tab;
    --tab;
}

std::string Emitter::prepare() { return root()->unique_name(); }

void Emitter::emit_epilogue(Lam* lam) {
    auto& bb = lam2bb_[lam];
    bb.tail("{}", emit(lam->body()));
}

void Emitter::finalize() {
    if (root()->sym().str() == "_fallback_compile") return;
    // We don't want to emit config lams that define which rules should be emitted.
    // The rules in the body of such a lambda will be emitted into rewrite_rules_
    // via emit_bb() but we don't want to emit the lambda itself.
    else if (root()->ret_dom()->sym().str() == "%eqsat.Rules")
        return;

    MutSet done;
    auto root_lam = nest().root()->mut()->as_mut<Lam>();
    emit_lam(root_lam, done);
}

std::string Emitter::emit_header(Lam* lam, bool as_binding) {
    std::ostringstream os;

    const std::string lam_kind = lam->isa_cn(lam) ? "con" : "lam";
    const std::string ext      = lam->is_external() ? "extern" : "intern";

    if (as_binding) {
        tab.lnprint(os, "(let {}", id(lam));
        ++tab;
        tab.lnprint(os, "({} {} {}", lam_kind, ext, id(lam));
    } else
        tab.print(os, "({} {} {}", lam_kind, ext, id(lam));

    ++tab;
    tab.lnprint(os, "(sigma");
    if (lam->has_var()) {
        ++tab;
        for (int i = 0; auto var : lam->vars()) {
            if (var) {
                tab.lnprint(os, "(var {}", id(var));
                ++tab;
                tab.lnprint(os, "{})", convert(var->type(), var));
                --tab;
            } else {
                tab.lnprint(os, "{}", convert(lam->dom(i)));
            }
            i++;
        }
        --tab;
    }
    print(os, ")");
    --tab;

    // Continuations have codomain .bot but lambdas can have arbitrary codomains
    if (!lam->isa_cn(lam)) {
        ++tab;
        tab.lnprint(os, "(sigma");
        ++tab;
        for (auto codom : lam->codoms())
            tab.lnprint(os, "{}", convert(codom));
        print(os, ")");
        --tab;
        --tab;
    }

    // emit_unsafe() for Defs that were already emitted uses caching which messes up indentation.
    // If we don't need basic blocks we just use emit_bb() with some dummy basic block instead.
    auto dummy = BB{};
    tab.print(os, "{}", emit_bb(dummy, lam->filter()));

    return os.str();
}

void Emitter::emit_lam(Lam* lam, MutSet& done) {
    done.emplace(lam);
    assert(lam2bb_.contains(lam));
    auto& bb = lam2bb_[lam];

    if (lam->is_closed())
        print(func_impls_, "{}", emit_header(lam));
    else
        print(func_impls_, "{}", emit_header(lam, true));

    ++tab;
    // Keeps count of parentheses opened by let-bindings that need to be closed later on
    auto unclosed_bindings = 0;
    for (auto op : lam->deps()) {
        for (auto mut : op->local_muts())
            if (auto next = nest()[mut]) {
                if (next->mut()->isa<Lam>() && !done.contains(next->mut())) {
                    auto next_lam = next->mut()->as_mut<Lam>();
                    emit_lam(next_lam, done);
                    unclosed_bindings++;
                }
            }
    }

    for (auto& term : bb.body()) {
        auto opened       = std::ranges::count(term.str(), '(');
        auto closed       = std::ranges::count(term.str(), ')');
        unclosed_bindings = unclosed_bindings + opened - closed;
        print(func_impls_, "{}", indented(tab.indent(), term.str()));
    }

    for (auto& term : bb.tail())
        print(func_impls_, "{}", indented(tab.indent(), term.str()));

    std::string closing_parens(unclosed_bindings, ')');
    print(func_impls_, "{}", closing_parens);
    --tab;

    if (lam->is_closed())
        print(func_impls_, ")\n\n");
    else {
        --tab;
        print(func_impls_, ")");
    }
}

std::string Emitter::emit_curried_app(BB& bb, const App& app) {
    std::ostringstream os;
    tab.lnprint(os, "(app ");

    if (auto app_callee = app.callee()->isa<App>()) {
        ++tab;
        tab.print(os, "{}", emit_curried_app(bb, *app_callee));
        --tab;
    } else
        tab.print(os, "{}", emit_bb(bb, app.callee()));

    tab.print(os, "{}", emit_bb(bb, app.arg()));

    print(os, ")");
    return os.str();
}
std::string Emitter::emit_bb(BB& bb, const Def* def) {
    std::ostringstream os;

    ++tab;
    if (def->type()->isa<Type>() || def->type()->isa<Univ>()) {
        tab.lnprint(os, "{}", convert(def));

    } else if (auto lam = def->isa<Lam>()) {
        tab.lnprint(os, "{}", id(lam));

    } else if (auto lit = def->isa<Lit>()) {
        if (lit->type()->isa<Nat>()) {
            std::string alias;
            auto nat_val = lit->get<u64>();
            switch (nat_val) {
                case 0x100: alias = "i8";
                case 0x1000: alias = "i16";
                case 0x100000000: alias = "i32";
                default: break;
            }
            if (!alias.empty())
                tab.lnprint(os, "(lit {})", alias);
            else
                tab.lnprint(os, "(lit {})", nat_val);
        } else if (auto size = Idx::isa(lit->type()))
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
        print(os, ")");

    } else if (auto seq = def->isa<Seq>()) {
        auto shape = seq->arity();
        auto body  = seq->body();
        if (seq->sym().empty()) {
            tab.lnprint(os, "(pack");
            tab.print(os, "{}", emit_bb(bb, shape));
            tab.print(os, "{}", emit_bb(bb, body));
            print(os, ")");
        } else {
            auto body_val  = emit_bb(bb, body);
            auto shape_val = emit_bb(bb, shape);
            bb.assign(tab, id(seq), [&](auto& os) {
                ++tab;
                tab.lnprint(os, "(pack");
                ++tab;
                tab.print(os, "{}", indented(tab.indent(), shape_val));
                tab.print(os, "{}", indented(tab.indent(), body_val));
                --tab;
                print(os, ")");
                --tab;
            });
            tab.lnprint(os, "{}", id(seq));
        }

    } else if (auto extract = def->isa<Extract>()) {
        auto tuple = extract->tuple();
        auto index = extract->index();

        // 'tuple' is another extract if we have for example two nested, sigma-typed variables
        // and we are trying to print a named projection of the inner variable. ('baz' in the example below)
        // ex.:  (var foo (sigma (var bar (sigma (var baz Nat)))))
        // In this example, we have an extract where the tuple: 'bar' is another extract from 'foo'.
        auto is_nested_proj = false;
        if (auto lit = Lit::isa(index); lit && tuple->isa<Extract>()) {
            auto curr_tuple = tuple;
            auto curr_index = index;
            while (curr_tuple != nullptr && curr_index != nullptr)
                if (auto lit = Lit::isa(curr_index); lit && curr_tuple->isa<Extract>()) {
                    curr_tuple = tuple->as<Extract>()->tuple();
                    curr_index = tuple->as<Extract>()->index();
                    continue;
                } else if (auto lit = Lit::isa(curr_index); lit && curr_tuple->isa<Var>()) {
                    is_nested_proj = true;
                    break;
                } else {
                    break;
                }
        }

        if (auto lit = Lit::isa(index); (lit && tuple->isa<Var>()) || is_nested_proj) {
            tab.lnprint(os, "{}", id(extract));
        } else if (extract->sym().empty()) {
            tab.lnprint(os, "(extract");
            tab.print(os, "{}", emit_bb(bb, tuple));
            tab.print(os, "{}", emit_bb(bb, index));
            print(os, ")");
        } else {
            auto tuple_val = emit_bb(bb, tuple);
            auto index_val = emit_bb(bb, index);
            bb.assign(tab, id(extract), [&](auto& os) {
                ++tab;
                tab.lnprint(os, "(extract");
                ++tab;
                tab.print(os, "{}", indented(tab.indent(), tuple_val));
                tab.print(os, "{}", indented(tab.indent(), index_val));
                --tab;
                print(os, ")");
                --tab;
            });
            tab.lnprint(os, "{}", id(extract));
        }

    } else if (auto insert = def->isa<Insert>()) {
        auto tuple = insert->tuple();
        auto index = insert->index();
        auto value = insert->value();
        if (insert->sym().empty()) {
            tab.lnprint(os, "(ins");
            tab.print(os, "{}", emit_bb(bb, tuple));
            tab.print(os, "{}", emit_bb(bb, index));
            tab.print(os, "{}", emit_bb(bb, value));
            print(os, ")");
        } else {
            auto tuple_val = emit_bb(bb, tuple);
            auto index_val = emit_bb(bb, index);
            auto value_val = emit_bb(bb, value);
            bb.assign(tab, id(insert), [&](auto& os) {
                ++tab;
                tab.lnprint(os, "(ins");
                ++tab;
                tab.print(os, "{}", indented(tab.indent(), tuple_val));
                tab.print(os, "{}", indented(tab.indent(), index_val));
                tab.print(os, "{}", indented(tab.indent(), value_val));
                --tab;
                print(os, ")");
                --tab;
            });
            tab.lnprint(os, "{}", id(insert));
        }

    } else if (auto var = def->isa<Var>()) {
        tab.lnprint(os, "{}", id(var));

    } else if (auto app = def->isa<App>()) {
        auto app_val = emit_curried_app(bb, *app);
        if (app->sym().empty()) {
            tab.print(os, "{}", app_val);
        } else {
            bb.assign(tab, id(app), [&](auto& os) {
                ++tab;
                tab.lnprint(os, "{}", indented(tab.indent(), app_val));
                --tab;
            });
            tab.lnprint(os, "{}", id(app));
        }

    } else if (auto axm = def->isa<Axm>()) {
        tab.lnprint(os, "{}", id(axm));

    } else if (def->isa<Bot>()) {
        if (def->sym().empty())
            tab.lnprint(os, "(bot {})", convert(def->type()));
        else {
            bb.assign(tab, id(def), "(bot {})", convert(def->type()));
            tab.lnprint(os, "{}", id(def));
        }

    } else if (def->isa<Top>()) {
        if (def->sym().empty())
            tab.lnprint(os, "(top {})", convert(def->type()));
        else {
            bb.assign(tab, id(def), "(top {})", convert(def->type()));
            tab.lnprint(os, "{}", id(def));
        }

    } else if (auto rule = def->isa<Rule>()) {
        // TODO: lhs and rhs are equal for some reason
        auto lhs_val   = emit_bb(bb, rule->lhs());
        auto rhs_val   = emit_bb(bb, rule->rhs());
        auto guard_val = emit_bb(bb, rule->guard());
        tab.lnprint(os, "(rule {} {} {})", lhs_val, rhs_val, guard_val);

        rewrite_rules_ << "(rule" << indented(1, lhs_val) << indented(1, rhs_val) << indented(1, guard_val) << ")\n\n";

    } else {
        error("Unhandled Def in SExpr backend: {} : {}", def, def->type());
    }
    --tab;

    return os.str();
}

void emit(World& world, std::ostream& ostream) {
    Emitter emitter(world, ostream);
    emitter.run();
}

} // namespace mim::sexpr
