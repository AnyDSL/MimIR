#include "thorin/util/sys.h"

#include <array>

#ifdef _WIN32
#    include <windows.h>
#    define popen  _popen
#    define pclose _pclose
#else
#    include <dlfcn.h>
#endif

namespace thorin::sys {

std::optional<std::filesystem::path> path_to_curr_exe() {
#ifdef _WIN32
    std::vector<char> path_buffer;
    size_t read = 0;
    do {
        // start with 256 (almost MAX_PATH) and grow exp
        path_buffer.resize(std::max(path_buffer.size(), static_cast<size_t>(128)) * 2);
        read = GetModuleFileNameA(nullptr, path_buffer.data(), static_cast<DWORD>(path_buffer.size()));
    } while (read == path_buffer.size()); // if equal, the buffer was too small.
    if (read != 0) {
        path_buffer.resize(read);
        return std::filesystem::path{path_buffer.data()}.parent_path().parent_path() / "lib";
    }
#else
    Dl_info info;
    if (dladdr(reinterpret_cast<void*>(&path_to_curr_exe), &info)) {
        return std::filesystem::path{info.dli_fname}.parent_path().parent_path() / "lib";
    }
#endif
    return {};
}

// see https://stackoverflow.com/a/478960
std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) { throw std::runtime_error("error: popen() failed!"); }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) { result += buffer.data(); }
    return result;
}

} // namespace thorin::sys
