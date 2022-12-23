#include "dialects/haskell/haskell.h"

#include <thorin/config.h>
#include <thorin/pass/pass.h>

#include "thorin/dialects.h"

#include "dialects/haskell/be/haskell_emit.h"

using namespace thorin;

extern "C" THORIN_EXPORT DialectInfo thorin_get_dialect_info() {
    return {"haskell", 
            [](Passes& passes) {
                register_phase<haskell::ocaml_phase, haskell::OCamlEmitter>(passes);//, std::cout);
            },
            [](Backends& backends) { backends["hs"] = &haskell::emit; }, nullptr};
}
