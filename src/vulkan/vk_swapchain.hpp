#pragma once
#include "vk_prelude.hpp"

namespace pop::vulkan {

class VulkanSwapchainImage {
public:
    VulkanSwapchainImage(vk::Image image, vk::Extent2D extent, vk::Format format, vk::raii::Semaphore&& imagePresentSemaphore);

    static auto from(vk::Image image, vk::Extent2D extent, vk::Format format) -> VulkanSwapchainImage;

    [[nodiscard]] constexpr auto vk_image()                const -> vk::Image { return m_image; }
    [[nodiscard]] constexpr auto extent()                  const -> vk::Extent2D { return m_extent; }
    [[nodiscard]] constexpr auto format()                  const -> vk::Format { return m_format; }
    [[nodiscard]] constexpr auto full_subresource_range()  const -> vk::ImageSubresourceRange { return { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }; }
    [[nodiscard]] constexpr auto image_present_semaphore() const -> const vk::raii::Semaphore& { return m_imagePresentSemaphore; }

private:
    vk::Image m_image;
    vk::Extent2D m_extent;
    vk::Format m_format;
    vk::raii::Semaphore m_imagePresentSemaphore;
};

class VulkanSwapchain {
public:
    VulkanSwapchain(vk::raii::SwapchainKHR&& swapchain, vk::Extent2D swapchain_image_extent, std::vector<VulkanSwapchainImage>&& swapchain_images);

    static auto create(vk::Extent2D swapchain_extent, std::optional<VulkanSwapchain>&& old_swapchain, bool vsync_enable) -> VulkanSwapchain;

    [[nodiscard]] auto vk_swapchain() const -> const vk::raii::SwapchainKHR& { return m_swapchain; }
    [[nodiscard]] auto image_extent() const -> const vk::Extent2D& { return m_swapchain_image_extent; }
    [[nodiscard]] auto images() const       -> const std::vector<VulkanSwapchainImage>& { return m_swapchain_images; }

private:
    vk::raii::SwapchainKHR m_swapchain;
    vk::Extent2D m_swapchain_image_extent;
    std::vector<VulkanSwapchainImage> m_swapchain_images;
};

} // namespace pop::vulkan