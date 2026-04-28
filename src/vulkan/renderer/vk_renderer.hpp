#pragma once
#include "vulkan/vk_prelude.hpp"
#include "vulkan/vk_swapchain.hpp"
#include <cstdint>

namespace pop::vulkan::renderer {

constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

struct FrameInFlight {
    vk::raii::Fence frame_finished_fence;

    vk::raii::Semaphore image_acquired_semaphore;

    vk::raii::CommandPool command_pool;
    vk::raii::CommandBuffer frame_command_buffer;
};

class VulkanRenderer {
public:
    VulkanRenderer(VulkanSwapchain&& swapchain, std::vector<FrameInFlight>&& frames_in_flight);
    ~VulkanRenderer();

    static auto create(VulkanSwapchain&& swapchain) -> VulkanRenderer;

    // TODO: rework to return some kind of result, for now false means need to recreate swapchain, true - not
    auto render_frame() -> bool;
private:
    VulkanSwapchain m_swapchain;

    std::vector<FrameInFlight> m_frames_in_flight;
    size_t m_current_frame = 0;
};

}