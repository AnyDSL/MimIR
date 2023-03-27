#pragma once

#include <memory>
#include <string>
#include <vector>

#include <absl/container/btree_map.h>
#include <absl/container/flat_hash_map.h>

#include "thorin/config.h"
#include "thorin/def.h"

namespace thorin {

class PipelineBuilder;

/// `axiom ↦ (pipeline part) × (axiom application) → ()` <br/>
/// The function should inspect App%lication to construct the Pass/Phase and add it to the pipeline.
using Passes      = absl::flat_hash_map<flags_t, std::function<void(World&, PipelineBuilder&, const Def*)>>;
using Backends    = absl::btree_map<std::string, void (*)(World&, std::ostream&)>;
using Normalizers = absl::flat_hash_map<flags_t, NormalizeFn>;

extern "C" {
/// Basic info and registration function pointer to be returned from a specific plugin.
/// Use Driver to load such a plugin.
struct Plugin {
    using Handle = std::unique_ptr<void, void (*)(void*)>;

    /// Name of the Plugin.
    const char* plugin_name;

    /// Callback for registering the Plugin's callbacks for the pipeline extension points.
    void (*register_passes)(Passes& passes);

    /// Callback for registering the mapping from backend names to emission functions in the given \a backends map.
    void (*register_backends)(Backends& backends);

    /// Callback for registering the mapping from axiom ids to normalizer functions in the given \a normalizers map.
    void (*register_normalizers)(Normalizers& normalizers);
};
}

/// To be implemented and exported by a plugin; shall return a filled Plugin.
extern "C" THORIN_EXPORT thorin::Plugin thorin_get_plugin();

} // namespace thorin
