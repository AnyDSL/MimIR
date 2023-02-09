#include "dialects/backend/be/ocaml_emit.h"

#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <ranges>
#include <string_view>

#include "thorin/axiom.h"
#include "thorin/def.h"
#include "thorin/tuple.h"

#include "thorin/analyses/cfg.h"
#include "thorin/analyses/deptree.h"
#include "thorin/be/emitter.h"
#include "thorin/fe/tok.h"
#include "thorin/util/assert.h"
#include "thorin/util/print.h"
#include "thorin/util/sys.h"

#include "absl/container/flat_hash_map.h"
#include "dialects/core/autogen.h"
#include "dialects/core/core.h"
#include "dialects/math/math.h"
#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

using namespace std::string_literals;

/*

TODO:
* mutual recursion
* operations
*/

namespace thorin::backend::ocaml {

const char* PREFACE = R"(
let bool2bit x = if x then 1 else 0
(* handle uninitialized values *)
(* let rec magic () : 'a = magic () *)
let unpack m = match m with
    | Some x -> x
    | None -> failwith "expected Some, got None"
let get_0_of_2 (x, _) = x (* fst *)
let get_1_of_2 (_, x) = x (* snd *)
let get_0_of_3 (x, _, _) = x
let get_1_of_3 (_, x, _) = x
let get_2_of_3 (_, _, x) = x
[@@@warning "-partial-match"]

)";

/*
 * helper
 */

static Def* isa_decl(const Def* def) {
    if (auto nom = def->isa_nom()) {
        if (nom->is_external() || nom->isa<Lam>() || (!nom->name().empty() && nom->name() != "_"s)) return nom;
    }
    return nullptr;
}

absl::flat_hash_map<const Def*, std::string> names;
static std::string id(const Def* def) {
    if (def->is_external() || (!def->is_set() && def->isa<Lam>())) return def->name();

    if (auto name = names.find(def); name != names.end()) return name->second;

    return def->unique_name();
}

static std::string_view external(const Def* def) {
    if (def->is_external()) return "(* external *)\n";
    return "";
}

/*
 * Inline + LRPrec
 */

/// This is a wrapper to dump a Def "inline" and print it with all of its operands.
struct Inline2 {
    Inline2(const Def* def, int dump_gid)
        : def_(def)
        , dump_gid_(dump_gid) {}
    Inline2(const Def* def)
        : Inline2(def, def->world().flags().dump_gid) {}

    const Def* operator->() const { return def_; };
    const Def* operator*() const { return def_; };
    explicit operator bool() const {
        if (def_->dep_const()) return true;

        if (auto nom = def_->isa_nom()) {
            if (isa_decl(nom)) return false;
            return true;
        }

        if (auto app = def_->isa<App>()) {
            if (app->type()->isa<Pi>()) return true; // curried apps are printed inline
            if (app->type()->isa<Type>()) return true;
            if (app->callee()->isa<Axiom>()) { return app->callee_type()->num_doms() <= 1; }
            return false;
        }

        return true;
    }

    friend std::ostream& operator<<(std::ostream&, Inline2);

private:
    const Def* def_;
    const int dump_gid_;
};

static std::string id(Inline2 u) { return id(*u); }

// TODO prec is currently broken
template<bool L>
struct LRPrec2 {
    LRPrec2(const Def* l, const Def* r)
        : l(l)
        , r(r) {}

    friend std::ostream& operator<<(std::ostream& os, const LRPrec2& p) {
        if constexpr (L) {
            if (Inline2(p.l) && fe::Tok::prec(fe::Tok::prec(p.r))[0] > fe::Tok::prec(p.r))
                return print(os, "({})", Inline2(p.l));
            return print(os, "{}", Inline2(p.l));
        } else {
            if (Inline2(p.r) && fe::Tok::prec(p.l) > fe::Tok::prec(fe::Tok::prec(p.l))[1])
                return print(os, "({})", Inline2(p.r));
            return print(os, "{}", Inline2(p.r));
        }
    }

private:
    const Def* l;
    const Def* r;
};

using LPrec = LRPrec2<true>;
using RPrec = LRPrec2<false>;

DefSet let_emitted;

// TODO: [@warning "-partial-match"]

std::ostream& operator<<(std::ostream& os, Inline2 u) {
    if (u.dump_gid_ == 2 || (u.dump_gid_ == 1 && !u->isa<Var>() && u->num_ops() != 0)) print(os, "/*{}*/", u->gid());

    if (let_emitted.contains(u.def_)) { return print(os, "{}", id(u)); }

    // Ptr -> Ref + Option for uninitialized
    // Arr -> List (need persistent)
    if (auto ptr = match<mem::Ptr>(*u)) {
        auto type = ptr->arg(0);
        return print(os, "({} option ref)", Inline2(type));
    }
    if (auto mem = match<mem::M>(*u)) { return print(os, "unit"); }
    // TODO: ptr(arr) -> array (but how to handle lea)

    if (auto arith = match<core::nop>(*u)) {
        auto [a, b] = arith->args<2>();
        const char* op;
        switch (arith.id()) {
            case core::nop::add: op = "+"; break;
            case core::nop::mul: op = "*"; break;
        }
        return print(os, "({} {} {})", Inline2(a), op, Inline2(b));
    }
    if (auto arith = match<core::wrap>(*u)) {
        auto [a, b] = arith->args<2>();
        // or use op+-* for others
        if (arith.id() == core::wrap::shl) { return print(os, "(Int.shift_left {} {})", Inline2(a), Inline2(b)); }
        const char* op;
        switch (arith.id()) {
            case core::wrap::add: op = "+"; break;
            case core::wrap::sub: op = "-"; break;
            case core::wrap::mul: op = "*"; break;
            case core::wrap::shl: unreachable();
        }
        return print(os, "({} {} {})", Inline2(a), op, Inline2(b));
    }
    if (auto icmp = match<core::icmp>(*u)) {
        auto [a, b] = icmp->args<2>();
        // or use op+-* for others
        const char* op;
        // TODO: signed unsigned difference
        switch (icmp.id()) {
            case core::icmp::e: op = "="; break;
            case core::icmp::sl: op = "<"; break;
            case core::icmp::sle: op = "<="; break;
            case core::icmp::ug: op = ">"; break;
            case core::icmp::uge: op = ">="; break;
            case core::icmp::ul: op = "<"; break;
            case core::icmp::ule: op = "<="; break;
            case core::icmp::sg: op = ">"; break;
            case core::icmp::sge: op = ">="; break;
            case core::icmp::ne: op = "!="; break;
            case core::icmp::f:
            case core::icmp::t: unreachable();
            default: assert(false && "unhandled icmp");
        }
        return print(os, "(bool2bit ({} {} {}))", Inline2(a), op, Inline2(b));
    }

    // div -> /
    // cmp -> bool2bit (...)

    if (auto app = u->isa<App>()) {
        auto callee = app->callee();
        if (callee->isa<Idx>()) { return print(os, "int"); }

        return print(os, "({} {})", Inline2(app->callee()), Inline2(app->arg()));
    }

    if (auto type = u->isa<Type>()) {
        auto level = as_lit(type->level()); // TODO other levels
        return print(os, level == 0 ? "★" : "□");
    } else if (u->isa<Nat>()) {
        return print(os, "int");
    } else if (u->isa<Idx>()) {
        assert(false && "idx should be handled by app");
    } else if (auto bot = u->isa<Bot>()) {
        if (bot->type()->isa<Type>()) return print(os, "'a");
        return print(os, "()");
    } else if (auto top = u->isa<Top>()) {
        if (top->type()->isa<Type>()) return print(os, "'a");
        return print(os, "()");
    } else if (auto axiom = u->isa<Axiom>()) {
        const auto& name = axiom->name();
        return print(os, "{}{}", name[0] == '%' ? "" : "%", name);
    } else if (auto lit = u->isa<Lit>()) {
        long value = lit->get();
        if (lit->type()->isa<Nat>()) return print(os, "{}", lit->get());
        if (value == 4294967295) { value = -1; }
        // TODO: 4294967295 => -1 / add modulo at operations (probably better)
        if (lit->type()->isa<App>())
            return print(os, "({}:{})", value, Inline2(lit->type())); // HACK prec magic is broken
        return print(os, "{}:{}", lit->get(), Inline2(lit->type()));
    } else if (auto ex = u->isa<Extract>()) {
        if (ex->tuple()->isa<Var>() && ex->index()->isa<Lit>()) return print(os, "{}", id(ex));
        if (ex->tuple()->type()->isa<Arr>()) {
            return print(os, "List.nth {} {}", Inline2(ex->tuple()), Inline2(ex->index()));
        } else {
            // TODO: extract from tuple (const index)
            auto extract_fun = "get_" + std::to_string(ex->index()->as<Lit>()->get()) + "_of_" +
                               std::to_string(ex->tuple()->num_ops());
            return print(os, "({} {})", extract_fun, Inline2(ex->tuple()));
        }
    } else if (auto var = u->isa<Var>()) {
        return print(os, "{}", id(var));
    } else if (auto pi = u->isa<Pi>()) {
        if (auto nom = pi->isa_nom<Pi>(); nom && nom->var()) assert(false && "nom pi");
        return print(os, "({} -> {})", Inline2(pi->dom()), Inline2(pi->codom()));
    } else if (auto lam = u->isa<Lam>()) {
        // TODO: lam vs lam->body() vs Inline(...) vs name vs unique_name
        return print(os, "{}", lam);
    } else if (auto sigma = u->isa<Sigma>()) {
        if (auto nom = sigma->isa_nom<Sigma>(); nom && nom->var()) {
            size_t i = 0;
            return print(os, "({, })", Elem(sigma->ops(), [&](auto op) {
                             if (auto v = nom->var(i++))
                                 print(os, "{}: {}", v, Inline2(op));
                             else
                                 print(os, "_: {}", Inline2(op));
                         }));
        }
        // TODO: add extra parens?
        if (sigma->num_ops() == 0) return print(os, "unit");
        return print(os, "({ * })", Elem(sigma->ops(), [&](auto op) { print(os, "{}", Inline2(op)); }));
    } else if (auto tuple = u->isa<Tuple>()) {
        if (tuple->type()->isa<Arr>()) {
            print(os, "[{; }]", Elem(tuple->ops(), [&](auto op) { print(os, "{}", Inline2(op)); }));
            return os;
            // TODO: nom
        } else {
            print(os, "({, })", Elem(tuple->ops(), [&](auto op) { print(os, "{}", Inline2(op)); }));
            return os;
        }
    } else if (auto arr = u->isa<Arr>()) {
        if (auto nom = arr->isa_nom<Arr>(); nom && nom->var())
            // TODO: impossible?
            return print(os, "«{}: {}; {}»", nom->var(), nom->shape(), nom->body());
        return print(os, "({} list)", Inline2(arr->body()));
    } else if (auto pack = u->isa<Pack>()) {
        // TODO: generate list
        if (auto nom = pack->isa_nom<Pack>(); nom && nom->var())
            return print(os, "(List.init {} (fun {} -> {}))", Inline2(nom->shape()), Inline2(nom->var()),
                         Inline2(nom->body()));
        return print(os, "(List.init {} (fun _ -> {}))", Inline2(pack->shape()), Inline2(pack->body()));

    } else if (auto proxy = u->isa<Proxy>()) {
        // TODO:
        assert(0);
    } else if (auto bound = u->isa<Bound>()) {
        // TODO:
        assert(0);
    }

    // other
    if (u->flags() == 0) return print(os, ".{} ({, })", u->node_name(), u->ops());
    return print(os, ".{}#{} ({, })", u->node_name(), u->flags(), u->ops());
}

/*
 * Dumper
 */

/// This thing operates in two modes:
/// 1. The output of decls is driven by the DepTree.
/// 2. Alternatively, decls are output as soon as they appear somewhere during recurse%ing.
///     Then, they are pushed to Dumper::noms.
class Dumper {
public:
    Dumper(std::ostream& os, const DepTree* dep = nullptr)
        : os(os)
        , dep(dep) {}

    void dump(Def*);
    void dump(Lam*);
    void dump_let(const Def*);
    void dump_ptrn(const Def*, const Def*, bool toplevel = true);
    void recurse(const DepNode*);
    void recurse(const Def*, bool first = false);

    std::ostream& os;
    const DepTree* dep;
    Tab tab;
    unique_queue<NomSet> noms;
    DefSet defs;
};

void Dumper::dump(Def* nom) {
    if (auto lam = nom->isa<Lam>()) {
        dump(lam);
        return;
    }

    auto nom_prefix = [&](const Def* def) {
        if (def->isa<Sigma>()) return ".Sigma";
        if (def->isa<Arr>()) return ".Arr";
        if (def->isa<Pack>()) return ".pack";
        if (def->isa<Pi>()) return ".Pi";
        unreachable();
    };

    auto nom_op0 = [&](const Def* def) -> std::ostream& {
        if (auto sig = def->isa<Sigma>()) return print(os, ", {}", sig->num_ops());
        if (auto arr = def->isa<Arr>()) return print(os, ", {}", arr->shape());
        if (auto pack = def->isa<Pack>()) return print(os, ", {}", pack->shape());
        if (auto pi = def->isa<Pi>()) return print(os, ", {}", pi->dom());
        unreachable();
    };

    if (!nom->is_set()) {
        tab.print(os, "{}: {} = {{ <unset> }};", id(nom), nom->type());
        return;
    }

    tab.print(os, "{} {}{}: {}", nom_prefix(nom), external(nom), id(nom), nom->type());
    nom_op0(nom);
    if (nom->var()) { // TODO rewrite with dedicated methods
        if (auto e = nom->num_vars(); e != 1) {
            print(os, "{, }", Elem(nom->vars(), [&](auto def) {
                      if (def)
                          os << id(def);
                      else
                          os << "<TODO>";
                  }));
        } else {
            print(os, ", @{}", id(nom->var()));
        }
    }
    tab.println(os, " = {{");
    ++tab;
    if (dep) recurse(dep->nom2node(nom));
    recurse(nom);
    tab.print(os, "{, }\n", nom->ops());
    --tab;
    tab.print(os, "}};\n");
}

void Dumper::dump(Lam* lam) {
    auto ptrn = [&](auto&) { dump_ptrn(lam->var(), lam->type()->dom()); };

    // TODO: handle mutual recursion
    auto name = id(lam);
    if (name == "main") name = "thorin_main";
    tab.println(os, "{}let rec {} {} = ", external(lam), name, ptrn); //, lam->type()->codom());
    // }

    ++tab;
    if (lam->is_set()) {
        if (dep) recurse(dep->nom2node(lam));
        recurse(lam->filter());
        recurse(lam->body(), true);
        // TODO:
        tab.print(os, "{}\n", Inline2(lam->body()));
    } else {
        tab.print(os, " <unset>\n");
    }
    --tab;

    // TODO: not for toplevel in a more semantic manner
    if (tab.indent() > 0) tab.print(os, "in\n");
}

void Dumper::dump_let(const Def* def) {
    // TODO: type vs Inline type
    tab.print(os, "let {}: {} = {} in\n", id(def), Inline2(def->type()), Inline2(def, 0));
    let_emitted.insert(def);
}

void Dumper::dump_ptrn(const Def* def, const Def* type, bool toplevel) {
    if (!def) {
        os << "_";
    } else {
        auto projs = def->projs();
        if ((projs.size() == 1 || std::ranges::all_of(projs, [](auto def) { return !def; }))) {
            if (toplevel) {
                print(os, "({} : {})", id(def), Inline2(type));
            } else {
                print(os, "{}", id(def));
            }
        } else {
            size_t i            = 0;
            const char* pattern = "((({, }) as {}): {})";
            if (def->type()->isa<Arr>()) pattern = "(([{; }] as {}): {})";
            print(os, pattern, Elem(projs, [&](auto proj) { dump_ptrn(proj, type->proj(i++), false); }), id(def),
                  Inline2(type));
        }
    }
}

void Dumper::recurse(const DepNode* node) {
    for (auto child : node->children()) {
        if (auto nom = isa_decl(child->nom())) dump(nom);
    }
}

void Dumper::recurse(const Def* def, bool first /*= false*/) {
    if (auto nom = isa_decl(def)) {
        if (!dep) noms.push(nom);
        return;
    }

    if (!defs.emplace(def).second) return;

    for (auto op : def->partial_ops().skip_front()) { // ignore dbg
        if (!op) continue;
        recurse(op);
    }

    if (!first && !Inline2(def)) dump_let(def);
}

void emit(World& w, std::ostream& os) {
    auto freezer = World::Freezer(w);

    auto dep    = DepTree(w);
    auto dumper = Dumper(os, &dep);

    os << PREFACE << "\n";
    dumper.recurse(dep.root());
}

} // namespace thorin::backend::ocaml
