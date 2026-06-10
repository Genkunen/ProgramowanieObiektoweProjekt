#pragma once
#include "vulkan/vk_image.hpp"

namespace pop::vulkan::renderer {

/// @struct RenderTargets
/// @brief Manages the render targets handles and information for the simulation.
struct RenderTargets {
    VulkanImage main_color_image;
    VulkanImage depth_buffer;
};

/// @class RenderTargetsManager
/// @brief Manages the render targets used by the simulation renderer.
class RenderTargetsManager {
public:
    RenderTargetsManager(RenderTargets&& render_targets);

    /// @brief Creates a new RenderTargetsManager instance.
    /// @param rt_image_extent The extent of the render target images.
    /// @returns A new RenderTargetsManager instance.
    static auto create(vk::Extent2D rt_image_extent) -> RenderTargetsManager;

    /// @brief Resizes the render targets to match the new window size.
    /// @param rt_image_extent The new extent of the render target images.
    /// @note This function should be called whenever the window size changes.
    auto resize_render_targets(vk::Extent2D rt_image_extent) -> void;

    [[nodiscard]] constexpr auto main_color_image() noexcept -> VulkanImage& { return m_render_targets.main_color_image; }
    [[nodiscard]] constexpr auto depth_buffer()     noexcept -> VulkanImage& { return m_render_targets.depth_buffer; }

private:
    static auto create_render_targets(vk::Extent2D rt_image_extent) -> RenderTargets;

    RenderTargets m_render_targets;
};

} // namespace pop::vulkan::renderer
