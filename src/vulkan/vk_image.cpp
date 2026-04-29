//
// Created by yorha2b on 2026/04/29.
//

#include "vk_image.hpp"

namespace pop::vulkan {

VulkanImage::VulkanImage(vk::raii::Image&& image, vk::raii::ImageView&& full_image_view, vma::raii::Allocation&& allocation, vk::Format format, vk::Extent3D extent)
    : m_image(std::move(image)), m_full_image_view(std::move(full_image_view)), m_allocation(std::move(allocation)), m_format(format), m_extent(extent) {}

} // namespace pop::vulkan