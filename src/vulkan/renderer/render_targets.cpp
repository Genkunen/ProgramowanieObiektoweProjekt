#include "render_targets.hpp"

namespace pop::vulkan::renderer {

RenderTargetsManager::RenderTargetsManager(RenderTargets&& render_targets) : m_render_targets(std::move(render_targets)) {}

auto RenderTargetsManager::create(vk::Extent2D rt_image_extent) -> RenderTargetsManager {
    auto render_targets = create_render_targets(rt_image_extent);

    return RenderTargetsManager(std::move(render_targets));
}

auto RenderTargetsManager::resize_render_targets(vk::Extent2D rt_image_extent) -> void {
    m_render_targets = create_render_targets(rt_image_extent);
}

auto RenderTargetsManager::create_render_targets(vk::Extent2D rt_image_extent) -> RenderTargets {
    auto main_render_target = VulkanImage::builder()
        .set_extent(vk::Extent3D(rt_image_extent, 1))
        .set_format(vk::Format::eR16G16B16A16Sfloat)
        .set_initial_layout(vk::ImageLayout::eUndefined)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .set_mip_levels(1)
        .set_tiling(vk::ImageTiling::eOptimal)
        .set_type(vk::ImageType::e2D)
        .set_usage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc)
        .build();

    auto depth_buffer = VulkanImage::builder()
        .set_extent(vk::Extent3D(rt_image_extent, 1))
        .set_format(vk::Format::eD32Sfloat)
        .set_initial_layout(vk::ImageLayout::eUndefined)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .set_mip_levels(1)
        .set_tiling(vk::ImageTiling::eOptimal)
        .set_type(vk::ImageType::e2D)
        .set_usage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
        .build();

    return RenderTargets {
        .main_color_image = std::move(main_render_target),
        .depth_buffer = std::move(depth_buffer)
    };
}

} // namespace pop::vulkan::renderer