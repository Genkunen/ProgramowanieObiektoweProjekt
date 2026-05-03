#include "imgui_layer.hpp"

#include "../vulkan/vk_context.hpp"

#include <SDL3/SDL_vulkan.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include <array>

namespace pop::imgui {

ImGuiLayer::ImGuiLayer(const pop::sdl::SdlWindow& window, const pop::vulkan::VulkanSwapchain& swapchain) 
    : m_descriptor_pool{ create_descriptor_pool() } {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForVulkan(window.get());

    const auto& context = pop::vulkan::VulkanContext::get();
    auto swapchain_image_count = static_cast<uint32_t>(swapchain.images().size());
    const VkFormat color_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    
    struct VulkanLoaderContext {
        PFN_vkGetInstanceProcAddr loader = nullptr;
        VkInstance instance = VK_NULL_HANDLE;
    };

    if (!SDL_Vulkan_LoadLibrary(nullptr)) {
        throw std::runtime_error("SDL_Vulkan_LoadLibrary failed");
    }

    auto get_instance_proc_addr = SDL_Vulkan_GetVkGetInstanceProcAddr();
    if (!get_instance_proc_addr) {
        throw std::runtime_error("SDL_Vulkan_GetVkGetInstanceProcAddr returned null");
    }

    auto loader = reinterpret_cast<PFN_vkGetInstanceProcAddr>(get_instance_proc_addr);

    VulkanLoaderContext loader_ctx{
        loader,
        *context.vk_instance()
    };

    if (!ImGui_ImplVulkan_LoadFunctions(
        VK_API_VERSION_1_4,
        [](const char* name, void* user_data) -> PFN_vkVoidFunction {
            auto* ctx = static_cast<VulkanLoaderContext*>(user_data);
            return ctx->loader(ctx->instance, name);
        },
        &loader_ctx)) {
        throw std::runtime_error("ImGui_ImplVulkan_LoadFunctions failed");
    }

    ImGui_ImplVulkan_InitInfo ii{
        .ApiVersion = VK_API_VERSION_1_4,
        .Instance = *context.vk_instance(),
        .PhysicalDevice = *context.vk_physical_device(),
        .Device = *context.vk_device(),
        .QueueFamily = context.vk_graphics_queue_family(),
        .Queue = *context.vk_graphics_queue(),
        .DescriptorPool = *m_descriptor_pool,
        .DescriptorPoolSize = {},
        .MinImageCount = swapchain_image_count,
        .ImageCount = swapchain_image_count,
        .PipelineCache = {},

        .PipelineInfoMain = {
            .RenderPass = {},
            .Subpass = 0,
            .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
            .ExtraDynamicStates = {},
            .PipelineRenderingCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
                .pNext = {},
                .viewMask = {},
                .colorAttachmentCount = 1,
                .pColorAttachmentFormats = &color_format,
                .depthAttachmentFormat = {},
                .stencilAttachmentFormat = {},
            },
        },

        .UseDynamicRendering = true,

        .Allocator = {},
        .CheckVkResultFn = {},
        .MinAllocationSize = {},

        .CustomShaderVertCreateInfo = {},
        .CustomShaderFragCreateInfo = {},
    };

    ImGui_ImplVulkan_Init(&ii);
}

auto ImGuiLayer::create_descriptor_pool() -> vk::raii::DescriptorPool {
    auto& device = pop::vulkan::VulkanContext::get().vk_device();

    std::array<vk::DescriptorPoolSize, 11> pool_sizes {
        vk::DescriptorPoolSize(vk::DescriptorType::eSampler, 1000),
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1000),
        vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, 1000),
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, 1000),
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformTexelBuffer, 1000),
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageTexelBuffer, 1000),
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1000),
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 1000),
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBufferDynamic, 1000),
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageBufferDynamic, 1000),
        vk::DescriptorPoolSize(vk::DescriptorType::eInputAttachment, 1000),
    };

    auto pool_info = vk::DescriptorPoolCreateInfo()
        .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setMaxSets(1000 * static_cast<uint32_t>(pool_sizes.size()))
        .setPoolSizes(pool_sizes);

    return device.createDescriptorPool(pool_info);
}

void ImGuiLayer::begin_frame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

auto ImGuiLayer::draw_data() -> ImDrawData* {
    ImGui::Render();
    return ImGui::GetDrawData();
}

void ImGuiLayer::shutdown() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

ImGuiLayer::~ImGuiLayer() {
    shutdown();
}


}

