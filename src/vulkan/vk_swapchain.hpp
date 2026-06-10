#pragma once
#include "vk_prelude.hpp"

namespace pop::vulkan {

/// @class VulkanSwapchainImage
/// @brief Represents a single image in a Vulkan swapchain.
class VulkanSwapchainImage {
public:
    VulkanSwapchainImage(vk::Image image, vk::Extent2D extent, vk::Format format, vk::raii::Semaphore&& imagePresentSemaphore);

    static auto from(vk::Image image, vk::Extent2D extent, vk::Format format) -> VulkanSwapchainImage;

    /// @brief Returns the underlying Vulkan image object.
    [[nodiscard]] constexpr auto vk_image()                const noexcept -> vk::Image { return m_image; }
    /// @brief Returns the extent of the swapchain image.
    [[nodiscard]] constexpr auto extent()                  const noexcept -> vk::Extent2D { return m_extent; }
    /// @brief Returns the format of the swapchain image.
    [[nodiscard]] constexpr auto format()                  const noexcept -> vk::Format { return m_format; }
    /// @brief Returns the subresource range for the entire swapchain image.
    [[nodiscard]] constexpr auto full_subresource_range()  const noexcept -> vk::ImageSubresourceRange { return { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }; }
    /// @brief Returns the semaphore used to signal when the swapchain image is ready to be presented.
    [[nodiscard]] constexpr auto image_present_semaphore() const noexcept -> const vk::raii::Semaphore& { return m_imagePresentSemaphore; }

private:
    vk::Image m_image;
    vk::Extent2D m_extent;
    vk::Format m_format;
    vk::raii::Semaphore m_imagePresentSemaphore;
};

/// @class VulkanSwapchain
/// @brief Wrapper around a Vulkan @c vk::raii::SwapchainKHR object.
class VulkanSwapchain {
public:
    VulkanSwapchain(vk::raii::SwapchainKHR&& swapchain, vk::Extent2D swapchain_image_extent, std::vector<VulkanSwapchainImage>&& swapchain_images);

    /// @brief Creates a new Vulkan swapchain.
    /// @details This function creates a new swapchain with the given extent, vsync enable state, and an optional old swapchain. Parameters such as swapchain
    ///     image count are determined automatically.
    /// @param swapchain_extent The extent of the swapchain images.
    /// @param old_swapchain The old swapchain, if any.
    /// @param vsync_enable Whether to enable vsync.
    /// @return The new swapchain.
    static auto create(vk::Extent2D swapchain_extent, std::optional<VulkanSwapchain>&& old_swapchain, bool vsync_enable) -> VulkanSwapchain;

    /// @brief Returns the underlying Vulkan swapchain object.
    [[nodiscard]] auto vk_swapchain() const noexcept -> const vk::raii::SwapchainKHR& { return m_swapchain; }
    /// @brief Returns the extent of the swapchain images.
    [[nodiscard]] auto image_extent() const noexcept -> const vk::Extent2D& { return m_swapchain_image_extent; }
    /// @brief Returns the list of the swapchain images.
    [[nodiscard]] auto images()       const noexcept -> const std::vector<VulkanSwapchainImage>& { return m_swapchain_images; }

private:
    vk::raii::SwapchainKHR m_swapchain;
    vk::Extent2D m_swapchain_image_extent;
    std::vector<VulkanSwapchainImage> m_swapchain_images;
};

} // namespace pop::vulkan
