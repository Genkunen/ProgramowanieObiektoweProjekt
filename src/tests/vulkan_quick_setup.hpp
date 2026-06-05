#pragma once
#include "../sdl/sdl_lib.hpp"
#include "../vulkan/vk_context.hpp"

#include <functional>

inline auto vulkan_quick_setup(const std::function<void()>& callback) {
    pop::sdl::initializeSdl();
    auto window = pop::sdl::SdlWindow("Vulkan quick setup window", 640, 480);
    auto vulkan_context = pop::vulkan::VulkanContext::create(window);

    callback();

    pop::sdl::terminateSdl();
}