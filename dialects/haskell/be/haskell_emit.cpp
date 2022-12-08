#include "dialects/haskell/be/haskell_emit.h"

#include <deque>
#include <fstream>
#include <iomanip>
#include <limits>
#include <ranges>

#include "thorin/analyses/cfg.h"
#include "thorin/be/emitter.h"
#include "thorin/util/print.h"
#include "thorin/util/sys.h"

#include "dialects/core/core.h"
#include "dialects/math/math.h"
#include "dialects/mem/mem.h"

using namespace std::string_literals;

namespace thorin::haskell {

void emit(World& world, std::ostream& ostream) {
    // Emitter emitter(world, ostream);
    // emitter.run();
    ostream << "Hello, world!" << std::endl;
}

} // namespace thorin::haskell
