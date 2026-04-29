#pragma once

#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#endif

namespace pop::filesystem {

inline constexpr class {
public:
    inline static auto operator()() -> std::filesystem::path {
#if defined(_WIN32)
        char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);
        return std::filesystem::path(buffer).parent_path();
#elif defined(__linux__)
        char buffer[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", buffer, PATH_MAX);
        if (count > 0) {
            return std::filesystem::path(std::string(buffer, count)).parent_path();
        }
        return std::filesystem::current_path();
#else
        return std::filesystem::current_path();
#endif
    }
} relative_path;

}
