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
#include "dialects/backend/be/ocaml_emit.h"
#include "dialects/core/autogen.h"
#include "dialects/core/core.h"
#include "dialects/math/math.h"
#include "dialects/mem/autogen.h"
#include "dialects/mem/mem.h"

using namespace std::string_literals;

namespace thorin::backend::ocaml {

const char* PREFACE_CODE = R"(
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

// test via:
// ./build/bin/thorin -d backend --backend ml --output - lit/backend/pow.thorin

struct unit {};

class Emitter : public thorin::Emitter<std::string, std::string, unit, Emitter> {
public:
    using Super = thorin::Emitter<std::string, std::string, unit, Emitter>;

    Emitter(World& world, std::ostream& ostream)
        : Super(world, "ocaml_emitter", ostream) {}

    bool is_valid(std::string_view s) { return !s.empty(); }
    void start() override;
    void emit_imported(Lam*);
    void emit_epilogue(Lam*);
    std::string emit_bb(unit&, const Def*);
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

void emit2(World& world, std::ostream& ostream) { Emitter(world, ostream).run(); }

std::string Emitter::id(const Def* def, bool force_bb /*= false*/) const {
    return "id";
    // std::string s("id");
    // return s;
    // return "";
    // std::ostringstream s;
    // s << "id";
    // return s.str();
}

std::string Emitter::convert(const Def* type) { return "convert_placeholder"; }

void Emitter::start() {
    Super::start();
    ostream() << type_decls_.str() << '\n';
    for (auto&& decl : decls_) ostream() << decl << '\n';
    ostream() << func_decls_.str() << '\n';
    ostream() << vars_decls_.str() << '\n';
    ostream() << func_impls_.str() << '\n';
}

void Emitter::emit_imported(Lam* lam) {}

std::string Emitter::prepare(const Scope& scope) { return "prepare_placeholder"; }

void Emitter::finalize(const Scope& scope) {}

void Emitter::emit_epilogue(Lam* lam) {}

std::string Emitter::emit_bb(unit& bb, const Def* def) { return "bb_placeholder"; }

} // namespace thorin::backend::ocaml
