#pragma once
#include "vulkan/vk_image.hpp"

namespace pop::vulkan::renderer {

struct RenderTargets {
    VulkanImage main_color_image;
    VulkanImage depth_buffer;
};

class RenderTargetsManager {
public:
    RenderTargetsManager(RenderTargets&& render_targets);

    static auto create(vk::Extent2D rt_image_extent) -> RenderTargetsManager;
    auto resize_render_targets(vk::Extent2D rt_image_extent) -> void;

    [[nodiscard]] constexpr auto main_color_image() noexcept -> VulkanImage& { return m_render_targets.main_color_image; }
    [[nodiscard]] constexpr auto depth_buffer() noexcept     -> VulkanImage& { return m_render_targets.depth_buffer; }

private:
    static auto create_render_targets(vk::Extent2D rt_image_extent) -> RenderTargets;

    RenderTargets m_render_targets;
};

} // namespace pop::vulkan::renderer
