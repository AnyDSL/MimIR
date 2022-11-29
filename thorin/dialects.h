#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "thorin/be/emitter.h"
#include "thorin/pass/pass.h"
#include "thorin/pass/pipelinebuilder.h"

#include "absl/container/flat_hash_map.h"

namespace thorin {

using Backends    = std::map<std::string, std::function<void(World&, std::ostream&)>>;
using Normalizers = absl::flat_hash_map<flags_t, Def::NormalizeFn>;

extern "C" {
/// Basic info and registration function pointer to be returned from a dialect plugin.
/// Use \ref Dialect to load such a plugin.
struct DialectInfo {
    /// Name of the plugin
    const char* plugin_name;

    /// Callback for registering the dialects' callbacks for the pipeline extension points.
    void (*add_passes)(PipelineBuilder& builder);
    void (*register_passes)(Passes& passes);

    /// Callback for registering the mapping from backend names to emission functions in the given \a backends map.
    void (*register_backends)(Backends& backends);

    /// Callback for registering the mapping from axiom ids to normalizer functions in the given \a normalizers map.
    void (*register_normalizers)(Normalizers& normalizers);
};
}

/// To be implemented and exported by the dialect plugins.
/// Shall return a filled DialectInfo.
extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info();

/// A thorin dialect.
/// This is used to load and manage a thorin dialect.
///
/// A plugin implementor should implement \ref thorin_get_dialect_info and \ref DialectInfo.
class Dialect {
public:
    /// Finds and loads a shared object file that implements the \a name thorin dialect.
    /// If \a name is an absolute path to a .so/.dll file, this is used.
    /// Otherwise, "name", "libthorin_name.so" (Linux, Mac), "thorin_name.dll" (Win)
    /// are searched for in the search paths:
    /// 1. \a search_paths, 2. env var \em THORIN_DIALECT_PATH, 3. "/path/to/executable"
    static Dialect load(const std::string& name, Span<std::string> search_paths);

    /// Name of the dialect.
    std::string name() const { return info_.plugin_name; }

    /// Shared object handle. Can be used with the functions from \ref dl.
    void* handle() { return handle_.get(); }

    /// Registers callbacks in the \a builder that extend the exposed PassMan's.
    void add_passes(PipelineBuilder& builder) const {
        if (info_.add_passes) info_.add_passes(builder);
    }
    void register_passes(Passes& passes) const {
        if (info_.register_passes) info_.register_passes(passes);
    }

    /// Registers the mapping from backend names to emission functions in the given \a backends map.
    void register_backends(Backends& backends) const {
        if (info_.register_backends) info_.register_backends(backends);
    }

    /// Registers the mapping from axiom ids to normalizer functions in the given \a normalizers map.
    void register_normalizers(Normalizers& normalizers) const {
        if (info_.register_normalizers) info_.register_normalizers(normalizers);
    }

private:
    explicit Dialect(const std::string& plugin_path, std::unique_ptr<void, void (*)(void*)>&& handle);

    DialectInfo info_;
    std::string plugin_path_;
    std::unique_ptr<void, void (*)(void*)> handle_;
};

std::vector<std::filesystem::path> get_plugin_search_paths(Span<std::string> user_paths);

} // namespace thorin
