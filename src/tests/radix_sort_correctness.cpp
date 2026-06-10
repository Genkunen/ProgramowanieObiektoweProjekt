#include <cstdint>

#include "vulkan/renderer/radix_sort.hpp"
#include "vulkan/renderer/simulation_render_graph_passes.hpp"
#include "vulkan/vk_prelude.hpp"
#include "vulkan_quick_setup.hpp"

#include <print>
#include <random>

using namespace pop::vulkan;
using namespace pop::vulkan::renderer;

std::vector<size_t> sizes = {
    1,
    2,
    7,
    255,
    256,
    257,
    1023,
    1024,
    1025,
    65535,
    65536,
    65537,
    1 << 20,
    1 << 24,
    (1 << 24) + 1
};

auto allocate_kv_buffer_for_sort(size_t size) -> VulkanBuffer {
    return VulkanBuffer::builder()
        .set_size(size * sizeof(uint32_t))
        .set_usage(vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst)
        .set_memory_usage(vma::MemoryUsage::eAutoPreferDevice)
        .map_for_sequential_write()
        .build();
}

auto generate_random_test_vector(size_t size) -> std::tuple<std::vector<uint32_t>, std::vector<uint32_t>> {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis(0, 0xffffffff);

    std::vector<uint32_t> keys(size);
    std::vector<uint32_t> values(size);
    for (size_t i = 0; i < size; i++) {
        keys[i] = dis(gen);
        values[i] = dis(gen);
    }
    return std::make_tuple(keys, values);
}

void radix_sort_test() {
    auto mock_mesh_pool = MeshPool::create(1, 1);
    auto mock_swapchain_image = VulkanSwapchainImage::from(nullptr, vk::Extent2D{}, vk::Format::eUndefined);

    std::println("Compiling radix sort pass...");
    auto radix_sort_pass = std::make_unique<SimulationAccelerationGridRadixSortPass>(SimulationAccelerationGridRadixSortPass::create());

    auto command_pool_create_info = vk::CommandPoolCreateInfo()
        .setQueueFamilyIndex(VulkanContext::get().vk_graphics_queue_family());
    auto command_pool = VulkanContext::get().vk_device().createCommandPool(command_pool_create_info);

    auto fence_create_info = vk::FenceCreateInfo();
    auto fence = VulkanContext::get().vk_device().createFence(fence_create_info);

    std::println("Creating command buffer...");

    auto cmd = std::move(VulkanContext::get().vk_device().allocateCommandBuffers({*command_pool, vk::CommandBufferLevel::ePrimary, 1})[0]);


    for (auto size : sizes) {
        std::println(" * Testing radix sort with size {}", size);
        std::println("   -> Preparing buffers");
        auto key_buffer = allocate_kv_buffer_for_sort(size);
        auto value_buffer = allocate_kv_buffer_for_sort(size);
        auto key_buffer_scratch = allocate_kv_buffer_for_sort(size);
        auto value_buffer_scratch = allocate_kv_buffer_for_sort(size);

        auto group_local_histograms_buffer_size = renderer::compute_memory_needed_for_radix_sort_group_histograms(size);
        auto group_local_histograms_buffer = allocate_kv_buffer_for_sort(group_local_histograms_buffer_size);

        auto global_histogram_buffer = allocate_kv_buffer_for_sort(256);

        std::println("   -> Generating test vector");
        auto [keys, values] = generate_random_test_vector(size);
        memcpy(key_buffer.memory_host_ptr(), keys.data(), keys.size() * sizeof(uint32_t));
        memcpy(value_buffer.memory_host_ptr(), values.data(), values.size() * sizeof(uint32_t));

        std::println("   -> Preparing commands");
        render_graph::PassResources pass_resources{};
        pass_resources.inject_buffer(render_graph::BufferResourceIdentifier::AccelerationGridSortKeys, key_buffer);
        pass_resources.inject_buffer(render_graph::BufferResourceIdentifier::AccelerationGridSortValues, value_buffer);
        pass_resources.inject_buffer(render_graph::BufferResourceIdentifier::AccelerationGridSortKeysScratch, key_buffer_scratch);
        pass_resources.inject_buffer(render_graph::BufferResourceIdentifier::AccelerationGridSortValuesScratch, value_buffer_scratch);
        pass_resources.inject_buffer(render_graph::BufferResourceIdentifier::AccelerationGridSortGlobalHistogram, global_histogram_buffer);
        pass_resources.inject_buffer(render_graph::BufferResourceIdentifier::AccelerationGridSortGroupLocalHistograms, group_local_histograms_buffer);

        SimulationSharedPassData state{
            .mesh_pool = mock_mesh_pool,
            .current_swapchain_image = mock_swapchain_image
        };
        state.object_count = size;

        auto cb_begin_info = vk::CommandBufferBeginInfo();
        cmd.begin(cb_begin_info);

        radix_sort_pass->invoke(cmd, state, pass_resources);

        VulkanPipelineBarriers::builder()
            .insert_memory_barrier(
                vk::PipelineStageFlagBits2::eComputeShader, vk::AccessFlagBits2::eShaderStorageWrite,
                vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostRead
            )
            .flush(cmd);

        cmd.end();

        auto command_buffer_info = vk::CommandBufferSubmitInfo{}
            .setCommandBuffer(cmd);

        auto submit_info = vk::SubmitInfo2()
            .setCommandBufferInfos(command_buffer_info);

        std::println("   -> Submitting");
        VulkanContext::get().vk_graphics_queue().submit2(submit_info, fence);

        std::println("   -> Waiting");
        VulkanContext::get().vk_device().waitForFences(*fence, true, std::numeric_limits<uint64_t>::max());
        VulkanContext::get().vk_device().resetFences(*fence);
        command_pool.reset();

        std::println("   -> Verifying");

        auto gpu_sorted_keys = std::span{reinterpret_cast<uint32_t*>(key_buffer.memory_host_ptr()), size};

        if (!std::ranges::is_sorted(gpu_sorted_keys))
            throw std::runtime_error("Test failed: incorrectly sorted");
    }
}

int main(int argc, char* argv[]) {
    vulkan_quick_setup(radix_sort_test);
}