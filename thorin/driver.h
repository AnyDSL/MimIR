#pragma once

#include <list>

#include "thorin/dialects.h"
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

    /// @name getters
    ///@{
    Flags& flags() { return flags_; }
    Log& log() { return log_; }
    World& world() { return world_; }
    ///@}

    /// @name search paths and dialect loading
    ///@{
    void add_search_path(fs::path path) {
        if (fs::exists(path) && fs::is_directory(path)) search_paths_.insert(insert_, fs::absolute(std::move(path)));
    }

    /// Search paths for dialect plugins are in the following order:
    /// 1. All further user-specified paths via Driver::add_search_path; paths added first will also be searched first.
    /// 2. Current working directory.
    /// 3. All paths specified in the environment variable `THORIN_DIALECT_PATH`.
    /// 4. `path/to/thorin.exe/../../lib/thorin`
    /// 5. `CMAKE_INSTALL_PREFIX/lib/thorin`
    const auto& search_paths() const { return search_paths_; }

    /// Finds and loads a shared object file that implements the Thorin dialect @p name.
    /// If \a name is an absolute path to a `.so`/`.dll` file, this is used.
    /// Otherwise, "name", "libthorin_name.so" (Linux, Mac), "thorin_name.dll" (Win)
    /// are searched for in Driver::search_paths().
    Dialect load(const std::string& name);
    ///@}

    const auto& normalizers() const { return normalizers_; }
    const auto& backends() const { return backends_; }

private:
    Flags flags_;
    Log log_;
    World world_;
    std::list<fs::path> search_paths_;
    std::list<fs::path>::iterator insert_ = search_paths_.end();
    Normalizers normalizers_;
    Backends backends_;
};

} // namespace thorin
