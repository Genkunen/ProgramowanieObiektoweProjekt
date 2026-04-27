#pragma once
#include "vk_prelude.hpp"
#include <SDL3/SDL_video.h>

namespace pop::vulkan {

class VulkanContext {
public:
    VulkanContext(vk::detail::DynamicLoader&& dynamic_loader, vk::raii::Context&& raii_context, vk::raii::Instance&& instance, vk::raii::SurfaceKHR&& surface,
        vk::raii::PhysicalDevice&& physical_device, vk::raii::Device&& device);

    static auto create(SDL_Window* window) -> VulkanContext;

private:
    vk::detail::DynamicLoader m_dynamic_loader;
    vk::raii::Context m_raii_context;

    vk::raii::Instance m_instance;
    vk::raii::SurfaceKHR m_surface;
    vk::raii::PhysicalDevice m_physical_device;
    vk::raii::Device m_device;

    static auto create_instance(vk::raii::Context& raii_context) -> vk::raii::Instance;
    static auto select_physical_device(const vk::raii::Instance& instance) -> vk::raii::PhysicalDevice;
    static auto create_device(const vk::raii::SurfaceKHR& surface, const vk::raii::PhysicalDevice& physical_device) -> vk::raii::Device;

};

} // namespace pop::vulkan