#pragma once

#if defined(_WIN32) || defined(_WIN64)
# define _CRT_SECURE_NO_WARNINGS
#endif

#include <cstdlib>
#include <cstring>

namespace pop::systems {

/// @brief Returns true if debug mode is enabled.
inline bool is_debug_enabled() {
#ifndef NDEBUG
    return true;
#endif

    const char* env_var = std::getenv("POP_DEBUG");
    return env_var != nullptr && strcmp(env_var, "1") == 0;
}

} // namespace pop::systems