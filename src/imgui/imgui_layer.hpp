#pragma once

#include "../vulkan/vk_swapchain.hpp"
#include "../sdl/sdl_window.hpp"

#include <vulkan/vulkan.hpp>

struct ImDrawData;

namespace pop::imgui {

class ImGuiLayer {
public:
    ImGuiLayer(const pop::sdl::SdlWindow& window, const pop::vulkan::VulkanSwapchain& swapchain);
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer(ImGuiLayer&&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(ImGuiLayer&&) = delete;

    [[nodiscard]]
    static auto create(const pop::sdl::SdlWindow& window, const pop::vulkan::VulkanSwapchain& swapchain) -> ImGuiLayer;
    void begin_frame();
    auto draw_data() -> ImDrawData*;
    void shutdown();

private:
    vk::raii::DescriptorPool m_descriptor_pool;

    auto create_descriptor_pool() -> vk::raii::DescriptorPool;
};

}

