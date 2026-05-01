#pragma once
#include "mesh_pool.hpp"
#include "vulkan/vk_buffer.hpp"
#include "vulkan/vk_compute_pipeline.hpp"
#include "vulkan/vk_graphics_pipeline.hpp"
#include "vulkan/vk_image.hpp"
#include "vulkan/vk_pipeline_layout.hpp"
#include "vulkan/vk_prelude.hpp"
#include "vulkan/vk_swapchain.hpp"
#include <cstdint>

struct ImDrawData;

namespace pop::vulkan::renderer {

constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

enum class RenderResult {
    Ok,
    SwapchainSuboptimal,
};

struct FrameInFlight {
    vk::raii::Fence frame_finished_fence;

    vk::raii::Semaphore image_acquired_semaphore;

    vk::raii::CommandPool command_pool;
    vk::raii::CommandBuffer frame_command_buffer;

    VulkanBuffer simulation_object_buffer;

    VulkanBuffer mesh_index_to_buffer_params_table;
    uint64_t mesh_index_to_buffer_params_table_generation = 0;
};

class VulkanRenderer {
public:
    VulkanRenderer(VulkanSwapchain&& swapchain, VulkanPipelineLayout&& triangle_pipeline_layout, VulkanGraphicsPipeline&& triangle_pipeline,
        VulkanPipelineLayout&& simulation_pipeline_layout, VulkanComputePipeline&& simulation_pipeline, VulkanPipelineLayout&& simulation_clear_pipeline_layout,
        VulkanComputePipeline&& simulation_clear_pipeline, VulkanBuffer&& simulation_draw_commands_buffer, VulkanBuffer&& simulation_draw_commands_count_buffer,
        VulkanImage&& main_render_target, VulkanImage&& depth_buffer, std::vector<FrameInFlight>&& frames_in_flight);
    ~VulkanRenderer();

    static auto create(VulkanSwapchain&& swapchain) -> VulkanRenderer;

    auto render_frame(MeshPool& mesh_pool, Mesh& sample_mesh, ImDrawData* draw_data) -> RenderResult;
    auto handle_surface_invalidation(vk::Extent2D new_window_extent) -> void;
    auto swapchain() const -> const VulkanSwapchain&;
private:
    VulkanSwapchain m_swapchain;

    VulkanPipelineLayout m_triangle_pipeline_layout;
    VulkanGraphicsPipeline m_triangle_pipeline;

    VulkanPipelineLayout m_simulation_pipeline_layout;
    VulkanComputePipeline m_simulation_pipeline;

    VulkanPipelineLayout m_simulation_clear_pipeline_layout;
    VulkanComputePipeline m_simulation_clear_pipeline;

    VulkanBuffer m_simulation_draw_commands_buffer;
    VulkanBuffer m_simulation_draw_commands_count_buffer;

    // ---- Render Targets -------------------------------------------------------------------------------------------------------------------------------------

    VulkanImage m_main_render_target;
    VulkanImage m_depth_buffer;

    std::vector<FrameInFlight> m_frames_in_flight;
    size_t m_current_frame = 0;

    auto run_gpgpu_simulation_step(vk::raii::CommandBuffer& cmd, FrameInFlight& frame, uint32_t object_count) -> void;
    auto run_main_renderpass(vk::raii::CommandBuffer& cmd, MeshPool& mesh_pool) -> void;
    auto run_imgui_renderpass(vk::raii::CommandBuffer& cmd, ImDrawData* draw_data) -> void;
    auto copy_main_render_target_to_swapchain_image(vk::raii::CommandBuffer& cmd, const VulkanSwapchainImage& swapchain_image) -> void;
};

}
