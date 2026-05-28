#pragma once
#include <cstdlib>
#include <cstring>

namespace pop::systems {

inline bool is_debug_enabled() {
#ifndef NDEBUG
    return true;
#endif

    const char* env_var = std::getenv("POP_DEBUG");
    return env_var != nullptr && strcmp(env_var, "1") == 0;
}

}