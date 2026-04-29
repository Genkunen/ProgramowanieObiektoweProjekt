#include "vk_context.hpp"

#include <SDL3/SDL_vulkan.h>
#include <ostream>
#include <unordered_set>
#include <print>

namespace pop::vulkan {

auto VulkanContext::get() -> VulkanContext& {
    return *g_vulkan_context;
}

VulkanContext::VulkanContext(vk::detail::DynamicLoader&& dynamic_loader, vk::raii::Context&& raii_context, vk::raii::Instance&& instance, vk::raii::SurfaceKHR&& surface,
        vk::raii::PhysicalDevice&& physical_device, vk::raii::Device&& device, vma::raii::Allocator&& vma_allocator, std::unordered_map<uint32_t, vk::raii::Queue>&& queue_storage,
        uint32_t graphics_queue_family, uint32_t present_queue_family)
    : m_dynamic_loader(std::move(dynamic_loader)), m_raii_context(std::move(raii_context)), m_instance(std::move(instance)), m_surface(std::move(surface)),
        m_physical_device(std::move(physical_device)), m_device(std::move(device)), m_vma_allocator(std::move(vma_allocator)), m_queue_storage(std::move(queue_storage)),
        m_graphics_queue_family(graphics_queue_family), m_present_queue_family(present_queue_family) {}
VulkanContext::~VulkanContext() {
    m_device.waitIdle();
}

auto VulkanContext::create(sdl::SdlWindow& window) -> std::unique_ptr<VulkanContext> {
    assert(g_vulkan_context == nullptr && "only one instance of VulkanContext may exist at any given time");
    // Load Vulkan dynamically.

    auto dynamic_loader = vk::detail::DynamicLoader();
    auto vkGetInstanceProcAddr = dynamic_loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    auto raii_context = vk::raii::Context();
    auto instance = create_instance(raii_context);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

    auto physical_device = select_physical_device(instance);

    auto raw_surface = window.vulkan_create_raw_surface(instance);

    auto surface = vk::raii::SurfaceKHR(instance, raw_surface);

    auto [graphics_queue_index, present_queue_index] = select_physical_device_queue_families(surface, physical_device);

    auto unique_queue_families = std::unordered_set<uint32_t>{graphics_queue_index, present_queue_index};
    auto device = create_device(physical_device, unique_queue_families);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

    auto queues = acquire_device_queues(device, unique_queue_families);
    auto vma_allocator = create_vma_allocator(instance, physical_device, device);

    auto ptr = std::make_unique<VulkanContext>(std::move(dynamic_loader), std::move(raii_context), std::move(instance), std::move(surface),
            std::move(physical_device), std::move(device), std::move(vma_allocator), std::move(queues), graphics_queue_index, present_queue_index);
    g_vulkan_context = ptr.get();
    return ptr;
}

auto VulkanContext::create_instance(vk::raii::Context& raii_context) -> vk::raii::Instance {
    auto application_info = vk::ApplicationInfo()
        .setApplicationVersion(vk::makeApiVersion(0, 0, 1, 0))
        .setPApplicationName("pop")
        .setEngineVersion(vk::makeApiVersion(0, 0, 1, 0))
        .setPEngineName("No Engine")
        .setApiVersion(vk::ApiVersion14);

    uint32_t instance_extension_count = 0;
    auto instance_extensions = SDL_Vulkan_GetInstanceExtensions(&instance_extension_count);

    auto instance_create_info = vk::InstanceCreateInfo()
        .setPApplicationInfo(&application_info)
        .setEnabledExtensionCount(instance_extension_count)
        .setPpEnabledExtensionNames(instance_extensions)
        .setEnabledLayerCount(0);

    return vk::raii::Instance(raii_context, instance_create_info);
}

auto VulkanContext::select_physical_device(const vk::raii::Instance& instance) -> vk::raii::PhysicalDevice {
    auto physical_devices = instance.enumeratePhysicalDevices();
    // TODO: replace with a smarter select function

    if (physical_devices.empty()) {
        throw std::runtime_error("No physical devices found!");
    }

    auto device_name = std::string(physical_devices[0].getProperties().deviceName);
    std::println("Selected device: {}", device_name);

    return physical_devices[0];
}

auto VulkanContext::select_physical_device_queue_families(const vk::raii::SurfaceKHR& surface, const vk::raii::PhysicalDevice& physical_device) -> std::tuple<uint32_t, uint32_t> {
    // For this simulation we only need a graphics queue and a present queue.

    auto physical_device_queue_props = physical_device.getQueueFamilyProperties();

    uint32_t graphics_queue_family = 0;
    uint32_t present_queue_family = 0;

    for (uint32_t i = 0; i < physical_device_queue_props.size(); i++) {
        auto queue_props = physical_device_queue_props[i];
        if (queue_props.queueFlags & vk::QueueFlagBits::eGraphics) {
            graphics_queue_family = i;
        }
        if (physical_device.getSurfaceSupportKHR(i, surface)) {
            present_queue_family = i;
        }
    }

    return { graphics_queue_family, present_queue_family };
}

auto VulkanContext::acquire_device_queues(const vk::raii::Device& device, const std::unordered_set<uint32_t>& unique_queues)
    -> std::unordered_map<uint32_t, vk::raii::Queue> {
    std::unordered_map<uint32_t, vk::raii::Queue> queue_storage;
    for (uint32_t queue_family : unique_queues) {
        queue_storage.emplace(queue_family, device.getQueue(queue_family, 0));
    }
    return queue_storage;
}

auto VulkanContext::create_device(const vk::raii::PhysicalDevice& physical_device, const std::unordered_set<uint32_t>& unique_queue_families) -> vk::raii::Device {
    static float queue_priority = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    queue_create_infos.reserve(unique_queue_families.size());
    for (uint32_t queue_family : unique_queue_families) {
        auto queue_create_info = vk::DeviceQueueCreateInfo()
            .setQueueFamilyIndex(queue_family)
            .setQueueCount(1)
            .setPQueuePriorities(&queue_priority);
        queue_create_infos.emplace_back(queue_create_info);
    }

    auto device_extensions = std::vector<const char*> { vk::KHRSwapchainExtensionName };

    auto vk12_features = vk::PhysicalDeviceVulkan12Features()
        .setBufferDeviceAddress(true);

    auto vk13_features = vk::PhysicalDeviceVulkan13Features()
        .setSynchronization2(true)
        .setDynamicRendering(true);

    auto vk14_features = vk::PhysicalDeviceVulkan14Features()
        .setMaintenance5(true);

    auto device_create_info = vk::DeviceCreateInfo()
        .setPEnabledExtensionNames(device_extensions)
        .setEnabledLayerCount(0)
        .setQueueCreateInfos(queue_create_infos);

    auto sc = vk::StructureChain{
        device_create_info,
        vk12_features,
        vk13_features,
        vk14_features
    };

    return physical_device.createDevice(sc.get<vk::DeviceCreateInfo>());
}

auto VulkanContext::create_vma_allocator(const vk::raii::Instance& instance, const vk::raii::PhysicalDevice& physical_device, const vk::raii::Device& device) -> vma::raii::Allocator {
    auto allocatorCreateInfo = vma::AllocatorCreateInfo{}
        .setPhysicalDevice(physical_device)
        .setVulkanApiVersion(vk::ApiVersion14)
        .setFlags(vma::AllocatorCreateFlagBits::eBufferDeviceAddress);

    return vma::raii::createAllocator(instance, device, allocatorCreateInfo);
}

} // namespace pop::vulkan