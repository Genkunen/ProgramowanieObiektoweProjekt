#pragma once
#include "vk_context.hpp"
#include "vk_prelude.hpp"

namespace pop::vulkan {

struct ImageFormatMetadata {
    vk::ImageAspectFlags aspect_flags;
};

// clang-format off
inline static std::unordered_map<vk::Format, ImageFormatMetadata> IMAGE_FORMAT_METADATA = {
    // Color render target / texture formats
    { vk::Format::eR16G16B16A16Sfloat,           { vk::ImageAspectFlagBits::eColor } },
    { vk::Format::eR8G8B8A8Srgb,                 { vk::ImageAspectFlagBits::eColor } },
    { vk::Format::eB8G8R8A8Srgb,                 { vk::ImageAspectFlagBits::eColor } },
    { vk::Format::eBc7SrgbBlock,                 { vk::ImageAspectFlagBits::eColor } },

    // Depth attachment formats
    { vk::Format::eD32Sfloat,                    { vk::ImageAspectFlagBits::eDepth } },
    { vk::Format::eD24UnormS8Uint,               { vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil } }
};
// clang-format on

class VulkanImageBuilder;
class VulkanImage {
public:
    VulkanImage(vk::raii::Image&& image, vk::raii::ImageView&& full_image_view, vma::raii::Allocation&& allocation, vk::Format format, vk::Extent3D extent);

    [[nodiscard]] constexpr static auto builder() -> VulkanImageBuilder;

    [[nodiscard]] auto vk_image() const -> const vk::raii::Image& { return m_image; }
    [[nodiscard]] auto vk_full_image_view() const -> const vk::raii::ImageView& { return m_full_image_view; }
    [[nodiscard]] auto vma_allocation() const -> const vma::raii::Allocation& { return m_allocation; }
    [[nodiscard]] auto format() const -> vk::Format { return m_format; }
    [[nodiscard]] auto extent() const -> vk::Extent3D { return m_extent; }
    [[nodiscard]] auto full_subresource_range() const -> vk::ImageSubresourceRange { return { IMAGE_FORMAT_METADATA.at(m_format).aspect_flags, 0, 1, 0, 1 }; }
private:
    vk::raii::Image m_image;
    vk::raii::ImageView m_full_image_view;
    vma::raii::Allocation m_allocation;

    vk::Format m_format;
    vk::Extent3D m_extent;
};

class VulkanImageBuilder {
public:
    constexpr VulkanImageBuilder() = default;

    [[nodiscard]] auto set_type(vk::ImageType type) noexcept -> VulkanImageBuilder& { m_image_create_info.imageType = type; return *this; }
    [[nodiscard]] auto set_extent(vk::Extent3D extent) noexcept -> VulkanImageBuilder& { m_image_create_info.extent = extent; return *this; }
    [[nodiscard]] auto set_mip_levels(uint32_t mip_levels) noexcept -> VulkanImageBuilder& { m_image_create_info.mipLevels = mip_levels; return *this; }
    [[nodiscard]] auto set_format(vk::Format format) noexcept -> VulkanImageBuilder& { m_image_create_info.format = format; return *this; }
    [[nodiscard]] auto set_tiling(vk::ImageTiling tiling) noexcept -> VulkanImageBuilder& { m_image_create_info.tiling = tiling; return *this; }
    [[nodiscard]] auto set_usage(vk::ImageUsageFlags usage) noexcept -> VulkanImageBuilder& { m_image_create_info.usage = usage; return *this; }
    [[nodiscard]] auto set_initial_layout(vk::ImageLayout initial_layout) noexcept -> VulkanImageBuilder& { m_image_create_info.initialLayout = initial_layout; return *this; }
    [[nodiscard]] auto set_memory_usage(vma::MemoryUsage memory_usage) noexcept -> VulkanImageBuilder& { m_allocation_create_info.usage = memory_usage; return *this; }
    [[nodiscard]] auto build() -> VulkanImage {
        uint32_t graphics_queue_family_index = VulkanContext::get().vk_graphics_queue_family();
        m_image_create_info.arrayLayers = 1;
        m_image_create_info.samples = vk::SampleCountFlagBits::e1;
        m_image_create_info.sharingMode = vk::SharingMode::eExclusive;
        m_image_create_info.setQueueFamilyIndices(graphics_queue_family_index);

        auto [allocation, image] = VulkanContext::get().vma_allocator().createImage(m_image_create_info, m_allocation_create_info).split();

        vk::ImageViewType image_view_type;
        switch (m_image_create_info.imageType) {
            case vk::ImageType::e1D: image_view_type = vk::ImageViewType::e1D; break;
            case vk::ImageType::e2D: image_view_type = vk::ImageViewType::e2D; break;
            case vk::ImageType::e3D: image_view_type = vk::ImageViewType::e3D; break;
            default: break;
        }

        auto image_view_create_info = vk::ImageViewCreateInfo()
            .setImage(image)
            .setViewType(image_view_type)
            .setFormat(m_image_create_info.format)
            .setSubresourceRange(vk::ImageSubresourceRange()
                .setAspectMask(IMAGE_FORMAT_METADATA.at(m_image_create_info.format).aspect_flags)
                .setBaseMipLevel(0)
                .setLevelCount(m_image_create_info.mipLevels)
                .setBaseArrayLayer(0)
                .setLayerCount(1));

        auto full_image_view = VulkanContext::get().vk_device().createImageView(image_view_create_info);

        return VulkanImage(std::move(image), std::move(full_image_view), std::move(allocation), m_image_create_info.format, m_image_create_info.extent);
    }

private:
    vk::ImageCreateInfo m_image_create_info;
    vma::AllocationCreateInfo m_allocation_create_info;
};

constexpr auto VulkanImage::builder() -> VulkanImageBuilder { return VulkanImageBuilder(); }

}
