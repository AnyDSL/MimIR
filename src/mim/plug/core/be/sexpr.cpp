#include <iostream>
#include <regex>
#include <sstream>

#include <mim/plug/core/be/sexpr.h>
#include <mim/plug/eqsat/eqsat.h>
#include <mim/plug/math/math.h>

#include "mim/def.h"

#include "mim/be/emitter.h"
#include "mim/util/print.h"

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
        swap(a.parts, b.parts);
    }

    std::array<std::deque<std::ostringstream>, 3> parts;
    std::set<std::string> assigned;
};

class Emitter : public mim::Emitter<std::string, std::string, BB, Emitter> {
public:
    using Super = mim::Emitter<std::string, std::string, BB, Emitter>;

    Emitter(World& world, std::ostream& ostream, bool slotted = false)
        : Super(world, "sexpr_emitter", ostream) {
        slotted_ = slotted;
    }

    bool is_valid(std::string_view s) { return !s.empty(); }
    void start() override;
    void emit_imported(Lam*);
    std::string prepare();
    void emit_epilogue(Lam*);
    void finalize();
    void emit_lam(Lam* lam, MutSet& done);
    std::string emit_var(BB& bb, const Def* var, const Def* type);
    std::string emit_head(BB& bb, Lam* lam, bool as_binding = false);
    std::string emit_type(BB& bb, const Def* type);
    std::string emit_cons(std::vector<std::string> op_vals);
    std::string emit_node(BB& bb, const Def* def, std::string node_name, bool variadic = false, bool with_type = false);
    std::string emit_bb(BB& bb, const Def* def);

private:
    std::string id(const Def*, bool is_var_use = false) const;
    std::string indent(size_t tabs, std::string term);
    std::string flatten(std::string term);

    // Determines whether the symbolic expression should
    // be emitted in a style that is compatible with slotted-egg.
    bool slotted() const { return slotted_; }
    bool slotted_;

    std::ostringstream decls_;
    std::ostringstream func_decls_;
    std::ostringstream func_impls_;
};

std::string Emitter::id(const Def* def, bool is_var_use) const {
    std::string prefix = slotted() ? "$" : "";
    std::string id;

    // In slotted-egg variable-uses need to be explicitly wrapped in a var node i.e. in λx.x (lam $x (var $x))
    auto var_wrap
        = [&](std::string id) { return slotted() && is_var_use && id.starts_with('$') ? "(var " + id + ")" : id; };

    // Axioms, rules, unset lambdas(imports) and externals need to be emitted without a uid
    if (def->isa<Axm>())
        id = def->sym().str();
    else if (def->isa<Rule>())
        id = def->sym().str();
    else if (def->isa<Lam>() && !def->is_set())
        id = def->sym().str();
    else if (def->is_external())
        id = def->sym().str();
    else
        id = prefix + def->unique_name();

    return var_wrap(id);
}

// Adjusts the base indentation of a term-string like
//
// "        (app
//              foo
//              bar
//          )"
//
// to the number of tabs specified with 'tabs' (i.e. for tabs=1)
//
// "    (app
//          foo
//          bar
//      )"
//
std::string Emitter::indent(size_t tabs, std::string term) {
    std::string indent(tabs * 4, ' ');
    std::string result;
    std::string line;

    while (!term.empty() && (term.front() == '\n' || term.front() == '\r'))
        term.erase(0, 1);

    std::stringstream term_stream(term);
    size_t min_indent = term.find_first_not_of(' ');
    while (std::getline(term_stream, line)) {
        // Skips empty lines
        if (line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
        result += "\n" + indent + line.substr(min_indent);
    }

    return result;
}

// Removes all indentation so a term-string like
//
// "    (app
//          foo
//          bar
//      )"
//
// becomes flattened like below
//
// "(app foo bar)"
//
std::string Emitter::flatten(std::string term) {
    term = std::regex_replace(term, std::regex("( {4})"), "");

    while (!term.empty() && (term.front() == '\n' || term.front() == '\r'))
        term.erase(0, 1);

    term = std::regex_replace(term, std::regex("(\\r|\\n)"), " ");
    return term;
}

void Emitter::start() {
    Super::start();

    ostream() << decls_.str();
    ostream() << func_decls_.str();
    ostream() << func_impls_.str();

    // TODO: Use pretty(sexpr, line_len) from the egg FFI
    // to pretty-print the sexpr either based on a switch from the cli
    // or as default after the sexpr backend has been completed
}

void Emitter::emit_imported(Lam* lam) {
    auto bb = BB();

    const std::string ext = lam->is_external() ? "extern" : "intern";
    // We assume that the lambda will be a continuation since imports
    // only exist via cfun and ccon which are both internally modelled as con
    print(func_decls_, "(con {} {}", ext, id(lam));
    print(func_decls_, "{})\n\n", emit_var(bb, lam->var(), lam->type()->dom()));
}

std::string Emitter::prepare() { return root()->unique_name(); }

void Emitter::emit_epilogue(Lam* lam) {
    auto& bb = lam2bb_[lam];
    bb.tail("{}", emit(lam->body()));
}

void Emitter::finalize() {
    if (root()->sym().str() == "_fallback_compile") return;
    // We don't want to emit config lams that define which rules should be emitted.
    // The rules in the body of such a lambda will be emitted into decls_
    // via emit_bb() but we don't want to emit the lambda itself.
    else if (Axm::isa<mim::plug::eqsat::Rules>(root()->ret_dom()))
        return;

    MutSet done;
    auto root_lam = nest().root()->mut()->as_mut<Lam>();
    emit_lam(root_lam, done);
}

void Emitter::emit_lam(Lam* lam, MutSet& done) {
    done.emplace(lam);
    assert(lam2bb_.contains(lam));
    auto& bb = lam2bb_[lam];

    if (lam->is_closed())
        print(func_impls_, "{}", emit_head(bb, lam));
    else
        print(func_impls_, "{}", emit_head(bb, lam, true));

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
        auto opened = std::ranges::count(term.str(), '(');
        auto closed = std::ranges::count(term.str(), ')');
        unclosed_bindings += opened - closed;
        print(func_impls_, "{}", indent(tab.indent(), term.str()));
    }

    for (auto& term : bb.tail())
        print(func_impls_, "{}", indent(tab.indent(), term.str()));

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

std::string Emitter::emit_var(BB& bb, const Def* var, const Def* type) {
    std::ostringstream os;

    if (slotted()) {
        ++tab;
        tab.lnprint(os, "{}", id(var));
        tab.lnprint(os, "{}", emit_type(bb, type));
        --tab;
        return os.str();
    }

    ++tab;
    auto projs = var->projs();
    if (projs.size() == 1 || std::ranges::all_of(projs, [](auto proj) { return proj->sym().empty(); }))
        tab.lnprint(os, "(var {} {})", id(var), emit_type(bb, type));
    else {
        tab.lnprint(os, "(var {}", id(var));
        size_t i = 0;
        for (auto proj : projs)
            tab.print(os, " {}", emit_var(bb, proj, type->proj(i++)));
        ++tab;
        tab.lnprint(os, "{})", emit_type(bb, type));
        --tab;
    }
    --tab;
    return os.str();
}

std::string Emitter::emit_head(BB& bb, Lam* lam, bool as_binding) {
    std::ostringstream os;

    // TODO: Is there ever a case where we would be emitting a lambda instead of a continuation
    // since the assertions in the Emitter visit() method seem to make this impossible?
    const std::string lam_kind = lam->isa_cn(lam) ? "con" : "lam";
    const std::string ext      = lam->is_external() ? "extern" : "intern";

    if (as_binding) {
        tab.lnprint(os, "(let {}", id(lam));
        ++tab;
        tab.lnprint(os, "({} {} {}", lam_kind, ext, id(lam));
    } else
        tab.print(os, "({} {} {}", lam_kind, ext, id(lam));

    tab.print(os, "{}", emit_var(bb, lam->var(), lam->type()->dom()));

    // Continuations have codomain .bot but lambdas can have arbitrary codomains
    if (!lam->isa_cn(lam)) {
        ++tab;
        tab.lnprint(os, "(sigma");
        ++tab;
        for (auto codom : lam->codoms())
            tab.lnprint(os, "{}", emit_type(bb, codom));
        print(os, ")");
        --tab;
        --tab;
    }

    tab.print(os, "{}", emit_bb(bb, lam->filter()));

    return os.str();
}

std::string Emitter::emit_type(BB& bb, const Def* type) {
    if (auto i = types_.find(type); i != types_.end()) return i->second;

    std::ostringstream os;
    if (type->isa<Nat>()) {
        print(os, "Nat");
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
        print(os, "(idx (lit {} Nat))", size);
    } else if (auto lit = type->isa<Lit>()) {
        if (lit->type()->isa<Nat>())
            print(os, "(lit {} Nat)", lit);
        else if (auto size = Idx::isa(lit->type()))
            if (auto lit_size = Idx::size2bitwidth(size); lit_size && *lit_size == 1)
                print(os, "(lit {} Bool)", lit);
            else
                print(os, "(lit {} {})", lit->get(), emit_type(bb, lit->type()));
        else
            print(os, "(lit {} {})", lit->get(), emit_type(bb, lit->type()));
    } else if (auto arr = type->isa<Arr>()) {
        if (auto arity = Lit::isa(arr->arity())) {
            u64 size = *arity;
            print(os, "(arr (lit {} Nat) {})", size, emit_type(bb, arr->body()));
        } else if (auto top = arr->arity()->isa<Top>()) {
            print(os, "(arr (top {}) {})", emit_type(bb, top->type()), emit_type(bb, arr->body()));
        } else {
            print(os, "(arr {} {})", flatten(emit_bb(bb, arr->arity())), emit_type(bb, arr->body()));
        }
    } else if (auto pi = type->isa<Pi>()) {
        if (Pi::isa_cn(pi))
            print(os, "(cn {})", emit_type(bb, pi->dom()));
        else
            print(os, "(pi {} {})", emit_type(bb, pi->dom()), emit_type(bb, pi->codom()));
    } else if (auto sigma = type->isa<Sigma>()) {
        print(os, "(sigma { })", Elem(sigma->ops(), [&](auto op) { print(os, "{}", emit_type(bb, op)); }));
    } else if (auto tuple = type->isa<Tuple>()) {
        print(os, "(tuple { })", Elem(tuple->ops(), [&](auto op) { print(os, "{}", emit_type(bb, op)); }));
    } else if (auto app = type->isa<App>()) {
        print(os, "(app {} {})", emit_type(bb, app->callee()), emit_type(bb, app->arg()));
    } else if (auto axm = type->isa<Axm>()) {
        print(os, "{}", id(axm));
        if (!world().flags2annex().contains(axm->flags()))
            print(decls_, "(axm {} {})\n\n", id(axm), emit_type(bb, axm->type()));
    } else if (auto var = type->isa<Var>()) {
        print(os, "{}", id(var));
    } else if (auto hole = type->isa<Hole>()) {
        print(os, "(hole {})", id(hole));
    } else if (auto extract = type->isa<Extract>()) {
        auto tuple = extract->tuple();
        auto index = extract->index();
        // See explanation for the same thing in emit_bb
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
        if (auto lit = Lit::isa(index); (lit && tuple->isa<Var>()) || is_nested_proj)
            print(os, "{}", id(extract));
        else
            print(os, "(extract {} {})", emit_type(bb, tuple), emit_type(bb, index));
    } else if (auto mType = type->isa<Type>()) {
        if (auto level = Lit::isa(mType->level())) {
            if (level == 0) print(os, "(type (lit 0 Univ))");
            if (level == 1) print(os, "(type (lit 1 Univ))");
        } else {
            print(os, "(type {})", emit_type(bb, mType->level()));
        }
    } else if (type->isa<Univ>()) {
        print(os, "Univ");
    } else if (auto reform = type->isa<Reform>()) {
        print(os, "(reform {})", emit_type(bb, reform->meta_type()));
    } else if (auto join = type->isa<Join>()) {
        print(os, "(join { })", Elem(join->ops(), [&](auto op) { print(os, "{}", emit_type(bb, op)); }));
    } else if (auto meet = type->isa<Meet>()) {
        print(os, "(meet { })", Elem(meet->ops(), [&](auto op) { print(os, "{}", emit_type(bb, op)); }));
    } else {
        error("unsupported type '{}'", type);
        fe::unreachable();
    }

    return types_[type] = os.str();
}

// This is primarily needed because slotted-egraphs don't support
// variadic enodes (yet?) so we have to represent those as nested cons lists
// i.e. for Tuple: (tuple (cons a (cons b nil)))
std::string Emitter::emit_cons(std::vector<std::string> op_vals) {
    std::ostringstream os;

    size_t op_idx = 0;
    for (auto op_val : op_vals) {
        ++tab;
        tab.lnprint(os, "(cons");
        ++tab;
        tab.print(os, "{}", indent(tab.indent(), op_val));
        --tab;
        if (op_idx == op_vals.size() - 1) tab.lnprint(os, "nil");
        --tab;

        op_idx++;
    }

    std::string closing_brackets(op_vals.size(), ')');
    print(os, "{}", closing_brackets);

    return os.str();
}

std::string Emitter::emit_node(BB& bb, const Def* def, std::string node_name, bool variadic, bool with_type) {
    std::ostringstream os;

    std::vector<std::string> op_vals;

    if (with_type) {
        if (auto type_val = emit_bb(bb, def->type()); !type_val.empty()) op_vals.push_back(type_val);
    }

    // This is a bit of an edge case? because the ops of a pack don't contain its arity
    if (auto pack = def->isa<Pack>())
        if (auto arity_val = emit_bb(bb, pack->arity()); !arity_val.empty()) op_vals.push_back(arity_val);

    for (auto op : def->ops())
        if (auto op_val = emit_bb(bb, op); !op_val.empty()) op_vals.push_back(op_val);

    if (def->sym().empty()) {
        tab.lnprint(os, "({}", node_name);

        if (slotted() && variadic)
            tab.print(os, "{}", emit_cons(op_vals));
        else
            for (auto op_val : op_vals)
                tab.print(os, "{}", op_val);

        print(os, ")");

    } else {
        bb.assign(tab, id(def), [&](auto& os) {
            ++tab;
            tab.lnprint(os, "({}", node_name);

            if (slotted() && variadic)
                tab.print(os, "{}", emit_cons(op_vals));
            else {
                ++tab;
                for (auto op_val : op_vals)
                    tab.print(os, "{}", indent(tab.indent(), op_val));
                --tab;
            }

            print(os, ")");
            --tab;
        });
        tab.lnprint(os, "{}", id(def, true));
    }

    return os.str();
}

std::string Emitter::emit_bb(BB& bb, const Def* def) {
    std::ostringstream os;

    ++tab;
    if (def->type()->isa<Type>() || def->type()->isa<Univ>()) {
        tab.lnprint(os, "{}", emit_type(bb, def));

    } else if (auto lam = def->isa<Lam>()) {
        tab.lnprint(os, "{}", id(lam, true));

    } else if (auto lit = def->isa<Lit>()) {
        if (lit->type()->isa<Nat>())
            tab.lnprint(os, "(lit {} Nat)", lit);
        else if (auto size = Idx::isa(lit->type()))
            if (auto lit_size = Idx::size2bitwidth(size); lit_size && *lit_size == 1)
                tab.lnprint(os, "(lit {} Bool)", lit);
            else
                tab.lnprint(os, "(lit {} {})", lit->get(), emit_type(bb, lit->type()));
        else
            tab.lnprint(os, "(lit {} {})", lit->get(), emit_type(bb, lit->type()));

    } else if (auto tuple = def->isa<Tuple>()) {
        tab.print(os, "{}", emit_node(bb, tuple, "tuple", true));

    } else if (auto pack = def->isa<Pack>()) {
        tab.print(os, "{}", emit_node(bb, pack, "pack"));

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
        if (!slotted() && ((Lit::isa(index) && tuple->isa<Var>()) || is_nested_proj))
            tab.lnprint(os, "{}", id(extract));
        else
            tab.print(os, "{}", emit_node(bb, extract, "extract"));

    } else if (auto insert = def->isa<Insert>()) {
        tab.print(os, "{}", emit_node(bb, insert, "insert"));

    } else if (auto var = def->isa<Var>()) {
        tab.lnprint(os, "{}", id(var, true));

    } else if (auto app = def->isa<App>()) {
        tab.print(os, "{}", emit_node(bb, app, "app"));

    } else if (auto axm = def->isa<Axm>()) {
        tab.lnprint(os, "{}", id(axm));
        if (!world().flags2annex().contains(axm->flags()))
            print(decls_, "(axm {} {})\n\n", id(axm), emit_type(bb, axm->type()));

    } else if (auto bot = def->isa<Bot>()) {
        if (bot->sym().empty())
            tab.lnprint(os, "(bot {})", emit_type(bb, bot->type()));
        else {
            bb.assign(tab, id(bot), "(bot {})", emit_type(bb, bot->type()));
            tab.lnprint(os, "{}", id(bot, true));
        }

    } else if (auto top = def->isa<Top>()) {
        if (top->sym().empty())
            tab.lnprint(os, "(top {})", emit_type(bb, top->type()));
        else {
            bb.assign(tab, id(top), "(top {})", emit_type(bb, top->type()));
            tab.lnprint(os, "{}", id(top, true));
        }

    } else if (auto rule = def->isa<Rule>()) {
        auto meta_var_val = emit_var(bb, rule->meta_var(), rule->meta_var()->type());
        auto lhs_val      = emit_bb(bb, rule->lhs());
        auto rhs_val      = emit_bb(bb, rule->rhs());
        auto guard_val    = emit_bb(bb, rule->guard());
        tab.lnprint(os, "{}", id(rule, true));
        print(decls_, "(rule {} {} {} {} {})\n\n", indent(1, id(rule)), indent(1, meta_var_val), indent(1, lhs_val),
              indent(1, rhs_val), indent(1, guard_val));

    } else if (auto inj = def->isa<Inj>()) {
        tab.print(os, "{}", emit_node(bb, inj, "inj", false, true));

    } else if (auto merge = def->isa<Merge>()) {
        tab.print(os, "{}", emit_node(bb, merge, "merge", true, true));

    } else if (auto match = def->isa<Match>()) {
        tab.print(os, "{}", emit_node(bb, match, "match", true));

    } else if (auto proxy = def->isa<Proxy>()) {
        auto type_val = emit_bb(bb, proxy->type());
        auto pass_val = proxy->pass();
        auto tag_val  = proxy->tag();
        std::vector<std::string> op_vals;
        for (auto op : proxy->ops())
            if (auto op_val = emit_bb(bb, op); !op_val.empty()) op_vals.push_back(op_val);

        if (proxy->sym().empty()) {
            tab.lnprint(os, "(proxy");
            tab.print(os, "{}", type_val);
            // pass_val and tag_val are not emitted via emit_bb and therefore have no
            // leading newlines and indentation levels so we add those here
            ++tab;
            tab.lnprint(os, "{}", pass_val);
            tab.lnprint(os, "{}", tag_val);
            --tab;
            for (auto op_val : op_vals)
                tab.print(os, "{}", op_val);
            print(os, ")");
        } else {
            bb.assign(tab, id(proxy), [&](auto& os) {
                ++tab;
                tab.lnprint(os, "(proxy");
                ++tab;
                tab.print(os, "{}", type_val);
                ++tab;
                tab.lnprint(os, "{}", pass_val);
                tab.lnprint(os, "{}", tag_val);
                --tab;
                for (auto op_val : op_vals)
                    tab.print(os, "{}", indent(tab.indent(), op_val));
                --tab;
                print(os, ")");
                --tab;
            });
            tab.lnprint(os, "{}", id(proxy, true));
        }

    } else {
        error("Unhandled Def in SExpr backend: {} : {}", def, def->type());
        fe::unreachable();
    }
    --tab;

    return os.str();
}

void emit(World& world, std::ostream& ostream) {
    Emitter emitter(world, ostream);
    emitter.run();
}

void emit_slotted(World& world, std::ostream& ostream) {
    Emitter emitter(world, ostream, true);
    emitter.run();
}

} // namespace mim::sexpr
