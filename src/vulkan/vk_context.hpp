#pragma once
#include "sdl/sdl_window.hpp"
#include "vk_prelude.hpp"

#include <SDL3/SDL_video.h>
#include <unordered_map>
#include <unordered_set>

namespace pop::vulkan {

class VulkanContext {
public:
    VulkanContext(vk::detail::DynamicLoader&& dynamic_loader, vk::raii::Context&& raii_context, vk::raii::Instance&& instance, vk::raii::SurfaceKHR&& surface,
        vk::raii::PhysicalDevice&& physical_device, vk::raii::Device&& device, vma::raii::Allocator&& vma_allocator, std::unordered_map<uint32_t, vk::raii::Queue>&& queue_storage,
        uint32_t graphics_queue_family, uint32_t present_queue_family, vk::PhysicalDeviceVulkan13Properties physical_device_vulkan13_properties, bool debug_utils_enabled);
    ~VulkanContext();

    static auto create(sdl::SdlWindow& window) -> std::unique_ptr<VulkanContext>;
    static auto get()                 noexcept -> VulkanContext&;

    [[nodiscard]] constexpr auto vk_instance() const noexcept              -> const vk::raii::Instance& { return m_instance; }
    [[nodiscard]] constexpr auto vk_surface() const noexcept               -> const vk::raii::SurfaceKHR& { return m_surface; }
    [[nodiscard]] constexpr auto vk_physical_device() const noexcept       -> const vk::raii::PhysicalDevice& { return m_physical_device; }
    [[nodiscard]] constexpr auto vk_device() const noexcept                -> const vk::raii::Device& { return m_device; }
    [[nodiscard]] constexpr auto vk_graphics_queue() const noexcept        -> const vk::raii::Queue& { return m_queue_storage.at(m_graphics_queue_family); }
    [[nodiscard]] constexpr auto vk_present_queue() const noexcept         -> const vk::raii::Queue& { return m_queue_storage.at(m_present_queue_family); }
    [[nodiscard]] constexpr auto vk_graphics_queue_family() const noexcept -> uint32_t { return m_graphics_queue_family; }
    [[nodiscard]] constexpr auto vk_present_queue_family() const noexcept  -> uint32_t { return m_present_queue_family; }
    [[nodiscard]] constexpr auto vma_allocator() const noexcept            -> const vma::raii::Allocator& { return m_vma_allocator; }

    [[nodiscard]] constexpr auto physical_device_vulkan13_properties() const noexcept -> const vk::PhysicalDeviceVulkan13Properties& { return m_physical_device_vulkan13_properties; }

    [[nodiscard]] constexpr auto debug_utils_enabled() const noexcept      -> bool { return m_debug_utils_enabled; }

private:
    vk::detail::DynamicLoader m_dynamic_loader;
    vk::raii::Context         m_raii_context;

    vk::raii::Instance        m_instance;
    vk::raii::SurfaceKHR      m_surface;
    vk::raii::PhysicalDevice  m_physical_device;
    vk::raii::Device          m_device;
    vma::raii::Allocator      m_vma_allocator;

    std::unordered_map<uint32_t, vk::raii::Queue> m_queue_storage;
    uint32_t m_graphics_queue_family{};
    uint32_t m_present_queue_family{};

    vk::PhysicalDeviceVulkan13Properties m_physical_device_vulkan13_properties;

    bool m_debug_utils_enabled;

    static auto instance_supports_debug_utils() -> bool;
    static auto create_instance(vk::raii::Context& raii_context, bool debug_utils_enable) -> vk::raii::Instance;
    static auto select_physical_device(const vk::raii::Instance& instance) -> vk::raii::PhysicalDevice;
    static auto select_physical_device_queue_families(const vk::raii::SurfaceKHR& surface, const vk::raii::PhysicalDevice& physical_device) -> std::tuple<uint32_t, uint32_t>;
    static auto get_physical_device_vulkan13_properties(const vk::raii::PhysicalDevice& physical_device) -> vk::PhysicalDeviceVulkan13Properties;
    static auto create_device(const vk::raii::PhysicalDevice& physical_device, const std::unordered_set<uint32_t>& unique_queue_families) -> vk::raii::Device;
    static auto acquire_device_queues(const vk::raii::Device& device, const std::unordered_set<uint32_t>& unique_queue_families) -> std::unordered_map<uint32_t, vk::raii::Queue>;
    static auto create_vma_allocator(const vk::raii::Instance& instance, const vk::raii::PhysicalDevice& physical_device, const vk::raii::Device& device) -> vma::raii::Allocator;
};

inline VulkanContext* g_vulkan_context{};

} // namespace pop::vulkan
