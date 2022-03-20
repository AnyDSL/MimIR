#ifndef THORIN_DIALECT_H
#define THORIN_DIALECT_H

#include <string>

#include "thorin/pass/pass.h"
#include "thorin/pass/pipelinebuilder.h"

namespace thorin {

extern "C" {
/// Basic info and registration function pointer to be returned from a dialect plugin.
struct DialectInfo {
    /// Name of the plugin
    std::string plugin_name;

    /// Callback for registering the dialects' callbacks for the pipeline extension points.
    void (*register_passes)(PipelineBuilder& builder);
};
}
} // namespace thorin

/// To be implemented and exported by the dialect plugins.
/// Shall return a filled DialectInfo.
extern "C" THORIN_EXPORT thorin::DialectInfo thorin_get_dialect_info();

#endif
