#ifndef THORIN_CLI_DIALECTS_H
#define THORIN_CLI_DIALECTS_H

#include <memory>
#include <string>
#include <vector>

#include "thorin/dialect.h"

#include "thorin/pass/pass.h"
#include "thorin/pass/pipelinebuilder.h"

namespace thorin {

/// A thorin dialect.
class Dialect {
public:
    static Dialect load_dialect_library(const std::string& name, const std::vector<std::string>& search_paths);

    std::string name() const { return info_.plugin_name; }

    void* handle() { return handle_.get(); }

    void register_passes(PipelineBuilder& man) const { info_.register_passes(man); }

private:
    explicit Dialect(const std::string& plugin_path, std::unique_ptr<void, void (*)(void*)>&& handle);

    DialectInfo info_;
    std::string plugin_path_;
    std::unique_ptr<void, void (*)(void*)> handle_;
};

void test_plugin(Dialect& dialect);

} // namespace thorin
#endif
