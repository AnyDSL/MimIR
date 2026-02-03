#pragma once

#include <list>

#include <absl/container/node_hash_map.h>

#include "mim/flags.h"
#include "mim/plugin.h"
#include "mim/world.h"

#include "mim/util/log.h"

namespace mim {

/// Some "global" variables needed all over the place.
/// Well, there are not really global - that's the point of this class.
class Driver : public fe::SymPool {
public:
    Driver();

    /// @name Getters
    ///@{
    Flags& flags() { return flags_; }
    const Flags& flags() const { return flags_; }
    Log& log() const { return log_; }
    World& world() { return world_; }
    ///@}

    /// @name Manage Search Paths
    /// Search paths for plugins are in the following order:
    /// 1. The empty path. Used as prefix to look into current working directory without resorting to an absolute path.
    /// 2. All further user-specified paths via Driver::add_search_path; paths added first will also be searched first.
    /// 3. All paths specified in the environment variable `MIM_PLUGIN_PATH`.
    /// 4. `path/to/mim.exe/../../lib/mim`
    /// 5. `CMAKE_INSTALL_PREFIX/lib/mim`
    ///@{
    const auto& search_paths() const { return search_paths_; }
    void add_search_path(fs::path path) {
        if (fs::exists(path) && fs::is_directory(path)) search_paths_.insert(insert_, std::move(path));
    }
    ///@}

    /// @name Manage Imports
    /// This is a list of pairs where each pair contains:
    /// 1. The `fs::path` used during import,
    /// 2. The name as Sym%bol used in the `import` directive or in Parser::import.
    ///@{
    class Imports {
    public:
        Imports(Driver& driver)
            : driver_(driver) {}

        /// @name Get imports
        ///@{
        const auto& path2sym() { return path2sym_; }
        auto paths() { return path2sym_ | std::views::keys; }
        auto syms() { return path2sym_ | std::views::values; }
        ///@}

        /// @name Iterators
        ///@{
        auto begin() const { return path2sym_.cbegin(); }
        auto end() const { return path2sym_.cbegin(); }
        ///@}

        /// Yields a `fs::path*`, if not already added that you can use in Loc%ation; returns `nullptr` otherwise.
        const fs::path* add(fs::path, Sym);

    private:
        Driver& driver_;
        std::deque<std::pair<fs::path, Sym>> path2sym_;
    };

    const Imports& imports() const { return imports_; }
    Imports& imports() { return imports_; }
    ///@}

    /// @name Load Plugin
    /// Finds and loads a shared object file that implements the MimIR Plugin @p name.
    /// If \a name is an absolute path to a `.so`/`.dll` file, this is used.
    /// Otherwise, "name", "libmim_name.so" (Linux, Mac), "mim_name.dll" (Win)
    /// are searched for in Driver::search_paths().
    ///@{
    void load(Sym name);
    void load(const std::string& name) { return load(sym(name)); }
    bool is_loaded(Sym sym) const { return lookup(plugins_, sym); }
    void* get_fun_ptr(Sym plugin, const char* name);

    template<class F>
    auto get_fun_ptr(Sym plugin, const char* name) {
        return reinterpret_cast<F*>(get_fun_ptr(plugin, name));
    }

    template<class F>
    auto get_fun_ptr(const char* plugin, const char* name) {
        return get_fun_ptr<F>(sym(plugin), name);
    }
    ///@}

    /// @name Manage Plugins
    /// All these lookups yield `nullptr` if the key has not been found.
    ///@{
    auto stage(flags_t flags) { return lookup(stages_, flags); }
    const auto& stages() const { return stages_; }
    auto normalizer(flags_t flags) const { return lookup(normalizers_, flags); }
    auto normalizer(plugin_t d, tag_t t, sub_t s) const { return normalizer(d | flags_t(t << 8u) | s); }
    auto backend(std::string_view name) { return lookup(backends_, name); }
    ///@}

private:
    // This must go *first* so plugins will be unloaded *last* in the d'tor; otherwise funny things might happen ...
    absl::node_hash_map<Sym, Plugin::Handle> plugins_;
    Flags flags_;
    mutable Log log_;
    World world_;
    std::list<fs::path> search_paths_;
    std::list<fs::path>::iterator insert_ = search_paths_.end();
    Backends backends_;
    Flags2Stages stages_;
    Normalizers normalizers_;
    Imports imports_;
};

#define GET_FUN_PTR(plugin, f) get_fun_ptr<decltype(f)>(plugin, #f)

} // namespace mim
