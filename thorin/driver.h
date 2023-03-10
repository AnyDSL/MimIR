#pragma once

#include <list>

#include "thorin/flags.h"
#include "thorin/world.h"

#include "thorin/util/log.h"
#include "thorin/util/sys.h"

namespace thorin {

/// Some "global" variables needed all over the place.
/// Well, there are not really global - that's the point of this class.
class Driver : public SymPool {
public:
    Driver();

    void add_search_path(fs::path path) {
        if (fs::is_directory(path))
            search_paths_.insert(insert_, fs::absolute(std::move(path)));
    }

    /// 1. \a search_paths, 2. env var \em THORIN_DIALECT_PATH, 3. "/path/to/executable"
    const auto& search_paths() const { return search_paths_; }

    Flags flags;
    Log log;
#if THORIN_ENABLE_CHECKS
    absl::flat_hash_set<uint32_t> breakpoints;
#endif
    World world;

private:
    std::list<fs::path> search_paths_;
    std::list<fs::path>::iterator insert_ = search_paths_.end();
};

} // namespace thorin
