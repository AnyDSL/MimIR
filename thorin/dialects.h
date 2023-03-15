#pragma once

#include <memory>
#include <string>
#include <vector>

#include <absl/container/btree_map.h>
#include <absl/container/flat_hash_map.h>

#include "thorin/def.h"

namespace thorin {

class PipelineBuilder;

using Backend     = std::function<void(World&, std::ostream&)>;
using Backends    = absl::btree_map<std::string, Backend>;
using Normalizers = absl::flat_hash_map<flags_t, NormalizeFn>;
/// `axiom ↦ (pipeline part) × (axiom application) → ()` <br/>
/// The function should inspect App%lication to construct the Pass/Phase and add it to the pipeline.
using Passes = absl::flat_hash_map<flags_t, std::function<void(World&, PipelineBuilder&, const Def*)>>;

extern "C" {
/// Basic info and registration function pointer to be returned from a dialect plugin.
/// Use \ref Dialect to load such a plugin.
struct DialectInfo {
    /// Name of the plugin
    const char* plugin_name;

    /// Callback for registering the dialects' callbacks for the pipeline extension points.
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
    using Handle = std::unique_ptr<void, void (*)(void*)>;

    /// Name of the dialect.
    std::string name() const { return info_.plugin_name; }

    /// Shared object handle. Can be used with the functions from \ref dl.
    void* handle() { return handle_.get(); }

    /// Registers callbacks in the \a builder that extend the exposed PassMan's.
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
    explicit Dialect(const std::string& plugin_path, Handle&&);

    DialectInfo info_;
    std::string plugin_path_;
    std::unique_ptr<void, void (*)(void*)> handle_;

    friend class Driver;
};

} // namespace thorin
