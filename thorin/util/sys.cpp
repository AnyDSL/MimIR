#include "thorin/util/sys.h"

#include <array>
#include <vector>

#ifdef _WIN32
#    include <windows.h>
#    define popen  _popen
#    define pclose _pclose
#elif defined(__APPLE__)
#    include <mach-o/dyld.h>
#    include <unistd.h>
#else
#    include <dlfcn.h>
#    include <unistd.h>
#endif

namespace thorin::sys {

std::optional<std::filesystem::path> path_to_curr_exe() {
    std::vector<char> path_buffer;
#ifdef __APPLE__
    uint32_t read = 0;
    _NSGetExecutablePath(nullptr, &read); // get size
    path_buffer.resize(read + 1);
    if(_NSGetExecutablePath(path_buffer.data(), &read) != 0)
        return {};
    return std::filesystem::path{path_buffer.data()};
#elif defined(_WIN32)
    size_t read = 0;
    do {
        // start with 256 (almost MAX_PATH) and grow exp
        path_buffer.resize(std::max(path_buffer.size(), static_cast<size_t>(128)) * 2);
        read = GetModuleFileNameA(nullptr, path_buffer.data(), static_cast<DWORD>(path_buffer.size()));
    } while (read == path_buffer.size()); // if equal, the buffer was too small.

    if (read != 0) {
        path_buffer.resize(read + 1);
        path_buffer.back() = 0;
        return std::filesystem::path{path_buffer.data()};
    }
#else // Linux only..
    if(std::filesystem::exists("/proc/self/exe"))
        return std::filesystem::canonical("/proc/self/exe");
#endif // __APPLE__
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
