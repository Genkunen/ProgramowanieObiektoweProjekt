#include "vk_swapchain.hpp"

#include "vk_context.hpp"

#include <ranges>

namespace pop::vulkan {

// ---- VulkanSwapchainImage -----------------------------------------------------------------------------------------------------------------------------------

VulkanSwapchainImage::VulkanSwapchainImage(vk::Image image, vk::Extent2D extent, vk::Format format, vk::raii::Semaphore&& imagePresentSemaphore)
    : m_image(image), m_extent(extent), m_format(format), m_imagePresentSemaphore(std::move(imagePresentSemaphore)) {}

auto VulkanSwapchainImage::from(vk::Image image, vk::Extent2D extent, vk::Format format) -> VulkanSwapchainImage {
    auto semaphore_create_info = vk::SemaphoreCreateInfo{};

    return VulkanSwapchainImage{ image, extent, format, VulkanContext::get().vk_device().createSemaphore(semaphore_create_info) };
}

// ---- VulkanSwapchain ----------------------------------------------------------------------------------------------------------------------------------------

VulkanSwapchain::VulkanSwapchain(vk::raii::SwapchainKHR&& swapchain, vk::Extent2D swapchain_image_extent,
                                 std::vector<VulkanSwapchainImage>&& swapchain_images)
                                     : m_swapchain(std::move(swapchain)), m_swapchain_image_extent(swapchain_image_extent), m_swapchain_images(std::move(swapchain_images)) {}

// clang-format off
static constexpr std::array<vk::PresentModeKHR, 4> PRESENT_MODE_PRIORITY_VSYNC = { vk::PresentModeKHR::eFifoRelaxed, vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eImmediate };
static constexpr std::array<vk::PresentModeKHR, 4> PRESENT_MODE_PRIORITY_NO_VSYNC = { vk::PresentModeKHR::eMailbox, vk::PresentModeKHR::eImmediate, vk::PresentModeKHR::eFifoRelaxed, vk::PresentModeKHR::eFifo };
// clang-format on

auto VulkanSwapchain::create(vk::Extent2D swapchain_extent, std::optional<VulkanSwapchain>&& old_swapchain, bool vsync_enable) -> VulkanSwapchain {
    auto& vk_physical_device = VulkanContext::get().vk_physical_device();
    auto& vk_surface = VulkanContext::get().vk_surface();
    auto surface_capabilities = vk_physical_device.getSurfaceCapabilitiesKHR(vk_surface);
    auto surface_formats = vk_physical_device.getSurfaceFormatsKHR(vk_surface);
    auto surface_present_modes = vk_physical_device.getSurfacePresentModesKHR(vk_surface);


    auto surface_format_iter = std::ranges::find_if(surface_formats, [&](const vk::SurfaceFormatKHR& sformat) -> bool {
        return (sformat.format == vk::Format::eR8G8B8A8Srgb || sformat.format == vk::Format::eB8G8R8A8Srgb) && sformat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });
    auto surface_format = surface_format_iter != surface_formats.end() ? *surface_format_iter : surface_formats[0];

    auto& present_mode_priority = vsync_enable ? PRESENT_MODE_PRIORITY_VSYNC : PRESENT_MODE_PRIORITY_NO_VSYNC;
    auto present_mode = *std::ranges::find_if(present_mode_priority, [&](const vk::PresentModeKHR& pmode) -> bool {
        return std::ranges::contains(surface_present_modes, pmode);
    });

    vk::Extent2D image_extent = surface_capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max() ? swapchain_extent : surface_capabilities.currentExtent;

    auto max_image_count = surface_capabilities.maxImageCount == 0 ? std::numeric_limits<uint32_t>::max() : surface_capabilities.maxImageCount;
    auto image_count = std::min(surface_capabilities.minImageCount + 1, max_image_count);
    auto image_sharing_mode = VulkanContext::get().vk_graphics_queue_family() == VulkanContext::get().vk_present_queue_family() ? vk::SharingMode::eExclusive : vk::SharingMode::eConcurrent;

    // TODO: this is kind of hacky, perhaps improve later
    auto unique_queue_families = std::vector<uint32_t>{ VulkanContext::get().vk_graphics_queue_family() };
    if (image_sharing_mode == vk::SharingMode::eConcurrent) {
        unique_queue_families.push_back(VulkanContext::get().vk_present_queue_family());
    }

    auto swapchain_create_info = vk::SwapchainCreateInfoKHR()
        .setSurface(VulkanContext::get().vk_surface())
        .setImageFormat(surface_format.format)
        .setImageColorSpace(surface_format.colorSpace)
        .setImageExtent(image_extent)
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eTransferDst)
        .setImageSharingMode(image_sharing_mode)
        .setMinImageCount(image_count)
        .setQueueFamilyIndices(unique_queue_families)
        .setPreTransform(surface_capabilities.currentTransform)
        .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
        .setClipped(true)
        .setPresentMode(present_mode);

    if (old_swapchain.has_value()) {
        swapchain_create_info.setOldSwapchain(old_swapchain->vk_swapchain());
    }

    auto swapchain = VulkanContext::get().vk_device().createSwapchainKHR(swapchain_create_info);
    auto prepared_swapchain_images = swapchain.getImages();
    auto swapchain_images = prepared_swapchain_images
        | std::views::transform([image_extent, surface_format](const vk::Image& image) { return VulkanSwapchainImage::from(image, image_extent, surface_format.format); })
        | std::ranges::to<std::vector<VulkanSwapchainImage>>();

    return VulkanSwapchain{ std::move(swapchain), image_extent, std::move(swapchain_images) };
}

} // namespace pop::vulkan