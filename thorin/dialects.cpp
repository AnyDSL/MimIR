#include "thorin/dialects.h"

#include <cstdlib>

#include <memory>
#include <sstream>
#include <string>

#include "thorin/config.h"
#include "thorin/driver.h"

#include "thorin/pass/pass.h"
#include "thorin/util/dl.h"

namespace thorin {

Dialect::Dialect(const std::string& plugin_path, Handle&& handle)
    : plugin_path_(plugin_path)
    , handle_(std::move(handle)) {
    auto get_info =
        reinterpret_cast<decltype(&thorin_get_dialect_info)>(dl::get(this->handle(), "thorin_get_dialect_info"));
    if (!get_info) err("plugin has no 'thorin_get_dialect_info()'");
    info_ = get_info();
}

} // namespace thorin
