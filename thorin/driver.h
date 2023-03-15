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
    void load(Sym name);
    void load(std::string name) { return load(sym(std::move(name))); }
    ///@}

    /// @name manage plugins
    ///@{
    /// All these lookups yield `nullptr` if the key has not been found.
    auto plugin(Sym sym) const { return lookup(plugins_, sym); }
    auto pass(flags_t flags) { return lookup(passes_, flags); }
    auto normalizer(flags_t flags) const { return lookup(normalizers_, flags); }
    auto normalizer(dialect_t d, tag_t t, sub_t s) const { return normalizer(d | flags_t(t << 8u) | s); }
    auto backend(std::string_view name) { return lookup(backends_, name); }
    ///@}

    /// Maps from absolute import path to actual usage in the source.
    absl::btree_map<fs::path, Sym> imports;

private:
    Flags flags_;
    Log log_;
    World world_;
    std::list<fs::path> search_paths_;
    std::list<fs::path>::iterator insert_ = search_paths_.end();
    absl::node_hash_map<Sym, Dialect> plugins_;
    Backends backends_;
    Passes passes_;
    Normalizers normalizers_;
};

} // namespace thorin
