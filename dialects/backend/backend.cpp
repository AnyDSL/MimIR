#include "dialects/backend/backend.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "dialects/backend/be/haskell_emit.h"
#include "dialects/backend/be/ocaml_emit.h"

using namespace thorin;

extern "C" THORIN_EXPORT DialectInfo thorin_get_dialect_info() {
    return {"backend",
            [](Passes& passes) {
                register_phase<backend::ocaml_phase, backend::OCamlEmitter>(passes); //, std::cout);
            },
            [](Backends& backends) {
                backends["ml"] = &backend::ocaml::emit;
                backends["hs"] = &backend::haskell::emit;
            },
            nullptr};
}
