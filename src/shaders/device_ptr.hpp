#pragma once

#ifdef __cplusplus
#include "vulkan/vk_prelude.hpp"
template <typename T> using DevicePtr = vk::DeviceAddress;
#else
typealias DevicePtr<T> = T*;
#endif