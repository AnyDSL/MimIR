#include "thorin/util/sys.h"

#include <array>
#include <iostream>
#include <vector>

#include "thorin/util/print.h"

#ifdef _WIN32
#    include <windows.h>
#    define popen  _popen
#    define pclose _pclose
#    define WEXITSTATUS
#else
#    include <dlfcn.h>
#    include <unistd.h>
#endif

using namespace std::string_literals;

namespace thorin::sys {

std::optional<std::filesystem::path> path_to_curr_exe() {
    std::vector<char> path_buffer;
    size_t read = 0;
    do {
        // start with 256 (almost MAX_PATH) and grow exp
        path_buffer.resize(std::max(path_buffer.size(), static_cast<size_t>(128)) * 2);
#ifdef _WIN32
        read = GetModuleFileNameA(nullptr, path_buffer.data(), static_cast<DWORD>(path_buffer.size()));
#else
        read = readlink("/proc/self/exe", path_buffer.data(), path_buffer.size());
#endif
    } while (read != size_t(-1) && read == path_buffer.size()); // if equal, the buffer was too small.
    if (read != 0 && read != size_t(-1)) {
#ifndef _WIN32
        read++;
#endif
        path_buffer.resize(read);
        path_buffer.back() = 0;
        return std::filesystem::path{path_buffer.data()}.parent_path().parent_path() / "lib";
    }
    return {};
}

// see https://stackoverflow.com/a/478960
std::string exec(std::string cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) throw std::runtime_error("error: popen() failed!");
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) { result += buffer.data(); }
    return result;
}

std::string find_cmd(std::string cmd) {
    auto out = exec(THORIN_WHICH " "s + cmd);
    out.erase(out.find('\n'));
    return out;
}

int system(std::string cmd) {
    std::cout << cmd << std::endl;
    int status = std::system(cmd.c_str());
    return WEXITSTATUS(status);
}

int run(std::string cmd, std::string args /* = {}*/) {
#ifdef _WIN32
    cmd += ".exe";
#else
    cmd = "./"s + cmd;
#endif
    return sys::system(cmd + " "s + args);
}

} // namespace thorin::sys
