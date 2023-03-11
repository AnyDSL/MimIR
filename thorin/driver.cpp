#include "thorin/driver.h"

#include "thorin/util/sys.h"

namespace thorin {

Driver::Driver()
    : log(*this)
    , world(this) {
    add_search_path(fs::current_path());

    // paths from env
    if (auto env_path = std::getenv("THORIN_DIALECT_PATH")) {
        std::stringstream env_path_stream{env_path};
        std::string sub_path;
        while (std::getline(env_path_stream, sub_path, ':')) add_search_path(sub_path);
    }

    // add path/to/thorin.exe/../../lib/thorin
    if (auto path = sys::path_to_curr_exe()) add_search_path(path->parent_path().parent_path() / "lib" / "thorin");

    // add install path if different from above
    if (auto install_path = fs::path{THORIN_INSTALL_PREFIX} / "lib" / "thorin"; fs::exists(install_path)) {
        if (search_paths().empty() || !fs::equivalent(install_path, search_paths().back()))
            add_search_path(std::move(install_path));
    }

    // all other user paths take precedence and are inserted before above fallbacks
    insert_ = search_paths_.begin();
}

} // namespace thorin
