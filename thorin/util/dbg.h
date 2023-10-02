#pragma once

#include <filesystem>

#include <fe/loc.h>
#include <fe/sym.h>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include "thorin/util/print.h"

namespace fe {

/// @name std::ostream operator
///@{
std::ostream& operator<<(std::ostream&, const Pos);
std::ostream& operator<<(std::ostream&, const Loc);
///@}

}

namespace thorin {

namespace fs = std::filesystem;

using fe::Loc;
using fe::Pos;
using fe::Sym;

/// @name Formatted Output
///@{
/// Prefixes error message with `<location>: error: `.
template<class T = std::logic_error, class... Args>
[[noreturn]] void error(Loc loc, const char* fmt, Args&&... args) {
    std::ostringstream o;
    print(o, "{}: error: ", loc);
    print(o, fmt, std::forward<Args&&>(args)...);
    throw T(o.str());
}
///@}

struct Dbg {
    Loc loc;
    Sym sym;
};

} // namespace thorin
