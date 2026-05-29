#pragma once

#ifdef __cplusplus
#include <vulkan/vulkan.hpp>

template <typename T> using DevicePtr = vk::DeviceAddress;
#else
typealias DevicePtr<T> = Ptr<T, Access.ReadWrite, AddressSpace.Device, ScalarDataLayout>;
#endif