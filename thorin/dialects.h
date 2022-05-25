#ifndef THORIN_DIALECTS_H
#define THORIN_DIALECTS_H

#include <memory>
#include <string>
#include <vector>

#include "thorin/be/emitter.h"
#include "thorin/pass/pass.h"
#include "thorin/pass/pipelinebuilder.h"

namespace thorin {

using Backends = std::map<std::string, std::function<void(World&, std::ostream&)>>;

extern "C" {
/// Basic info and registration function pointer to be returned from a dialect plugin.
/// Use \ref Dialect to load such a plugin.
struct DialectInfo {
    /// Name of the plugin
    const char* plugin_name;

    /// Callback for registering the dialects' callbacks for the pipeline extension points.
    void (*register_passes)(PipelineBuilder& builder);

    void (*register_backends)(Backends& backends);
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
    static Dialect load(const std::string& name, const std::vector<std::string>& search_paths);

    /// Name of the dialect.
    std::string name() const { return info_.plugin_name; }

    /// Shared object handle. Can be used with the functions from \ref dl.
    void* handle() { return handle_.get(); }

    /// Registers callbacks in the \a builder that extend the exposed PassMan's.
    void register_passes(PipelineBuilder& builder) const { info_.register_passes(builder); }

    /// Registers the mapping from backend names to emission functions in the given \a backends map.
    void register_backends(Backends& backends) const {
        if (info_.register_backends) info_.register_backends(backends);
    }

private:
    explicit Dialect(const std::string& plugin_path, std::unique_ptr<void, void (*)(void*)>&& handle);

    DialectInfo info_;
    std::string plugin_path_;
    std::unique_ptr<void, void (*)(void*)> handle_;
};

} // namespace thorin
#endif
