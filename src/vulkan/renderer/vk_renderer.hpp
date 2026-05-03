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
constexpr uint64_t DEFAULT_GPU_DRIVEN_SIM_OBJECT_COUNT = 1000;

enum class RenderResult {
    Ok,
    SwapchainSuboptimal,
};

enum class SimulationObjectBufferReadWriteDirection {
    e1to2,
    e2to1,
};

struct FrameInFlight {
    vk::raii::Fence frame_finished_fence;

    vk::raii::Semaphore image_acquired_semaphore;

    vk::raii::CommandPool command_pool;
    vk::raii::CommandBuffer frame_command_buffer;

    // Holds data about the simulation this frame (camera matrix, time deltas, etc.)
    VulkanBuffer simulation_data_buffer;
    // Staging buffer for data on mesh parameters to set up draw commands with.
    VulkanBuffer mesh_allocations_table_staging_buffer;

    // TODO: CPU-based simulation needs its own buffers

};

class VulkanRenderer {
public:
    VulkanRenderer(
        VulkanSwapchain&& swapchain, VulkanPipelineLayout&& triangle_pipeline_layout, VulkanGraphicsPipeline&& triangle_pipeline,
        VulkanPipelineLayout&& upload_meshes_cs_layout, VulkanComputePipeline&& upload_meshes_cs,
        VulkanPipelineLayout&& clear_instance_count_cs_layout, VulkanComputePipeline&& clear_instance_count_cs,
        VulkanPipelineLayout&& simulation_step_cs_layout, VulkanComputePipeline&& simulation_step_cs,
        VulkanPipelineLayout&& build_indirect_instance_count_cs_layout, VulkanComputePipeline&& build_indirect_instance_count_cs,
        VulkanPipelineLayout&& build_indirect_first_instance_cs_layout, VulkanComputePipeline&& build_indirect_first_instance_cs,
        VulkanPipelineLayout&& build_instance_buffer_cs_layout, VulkanComputePipeline&& build_instance_buffer_cs,
        VulkanBuffer&& simulation_objects_buffer_pp1, VulkanBuffer&& simulation_objects_buffer_pp2,
        VulkanBuffer&& indirect_draw_commands_buffer, VulkanBuffer&& drawlocal_instance_ids_buffer, VulkanBuffer&& instance_data_buffer,
        VulkanImage&& main_render_target, VulkanImage&& depth_buffer,
        std::vector<FrameInFlight>&& frames_in_flight);
    ~VulkanRenderer();

    static auto create(VulkanSwapchain&& swapchain) -> VulkanRenderer;

    auto render_frame(MeshPool& mesh_pool, const std::span<const Mesh>& meshes, ImDrawData* draw_data, float delta_time) -> RenderResult;
    auto handle_surface_invalidation(vk::Extent2D new_window_extent) -> void;
    auto swapchain() const -> const VulkanSwapchain&;

    auto reset_simulation_object_count(uint32_t new_count) -> void;

private:
    VulkanSwapchain m_swapchain;

    VulkanPipelineLayout m_triangle_pipeline_layout;
    VulkanGraphicsPipeline m_triangle_pipeline;

    VulkanPipelineLayout m_upload_meshes_cs_layout;
    VulkanComputePipeline m_upload_meshes_cs;

    VulkanPipelineLayout m_clear_instance_count_cs_layout;
    VulkanComputePipeline m_clear_instance_count_cs;

    VulkanPipelineLayout m_simulation_step_cs_layout;
    VulkanComputePipeline m_simulation_step_cs;

    VulkanPipelineLayout m_build_indirect_instance_count_cs_layout;
    VulkanComputePipeline m_build_indirect_instance_count_cs;

    VulkanPipelineLayout m_build_indirect_first_instance_cs_layout;
    VulkanComputePipeline m_build_indirect_first_instance_cs;

    VulkanPipelineLayout m_build_instance_buffer_cs_layout;
    VulkanComputePipeline m_build_instance_buffer_cs;

    // Ping-pong buffers for simulation objects data. The simulation step cycles between them.
    VulkanBuffer m_simulation_objects_buffer_pp1;
    VulkanBuffer m_simulation_objects_buffer_pp2;
    SimulationObjectBufferReadWriteDirection m_simulation_objects_buffer_rw_direction = SimulationObjectBufferReadWriteDirection::e1to2;
    // Holds indirect draw commands.
    VulkanBuffer m_indirect_draw_commands_buffer;
    uint64_t m_indirect_draw_commands_mesh_params_generation = 0;
    // Holds draw-local instance IDs of objects.
    VulkanBuffer m_drawlocal_instance_ids_buffer;
    // Holds coalesced instance data.
    VulkanBuffer m_instance_data_buffer;

    // ---- Render Targets -------------------------------------------------------------------------------------------------------------------------------------

    VulkanImage m_main_render_target;
    VulkanImage m_depth_buffer;

    std::vector<FrameInFlight> m_frames_in_flight;
    size_t m_current_frame = 0;

    uint32_t m_gpu_driven_sim_object_count = DEFAULT_GPU_DRIVEN_SIM_OBJECT_COUNT;
    bool m_gpu_driven_sim_needs_preinit = true;
    bool m_gpu_driven_sim_needs_refit = false; // initially prefitted to DEFAULT_GPU_DRIVEN_SIM_OBJECT_COUNT

    std::chrono::time_point<std::chrono::high_resolution_clock> m_start_timepoint = std::chrono::high_resolution_clock::now();

    auto refit_simulation_buffers() -> void;
    auto preinitialize_simulation(const std::span<const Mesh>& meshes) -> void;

    auto prepare_indirect_draw_mesh_params(vk::raii::CommandBuffer& cmd, FrameInFlight& frame, MeshPool& mesh_pool) -> void;
    auto clear_indirect_draw_instance_counts(vk::raii::CommandBuffer& cmd, MeshPool& mesh_pool) -> void;
    auto run_gpgpu_simulation_step(vk::raii::CommandBuffer& cmd, FrameInFlight& frame) -> void;
    auto build_indirect_draw_buffers(vk::raii::CommandBuffer& cmd, MeshPool& mesh_pool, FrameInFlight& frame) -> void;
    auto run_main_renderpass(vk::raii::CommandBuffer& cmd, MeshPool& mesh_pool, FrameInFlight& frame) -> void;
    auto run_imgui_renderpass(vk::raii::CommandBuffer& cmd, ImDrawData* draw_data) -> void;
    auto copy_main_render_target_to_swapchain_image(vk::raii::CommandBuffer& cmd, const VulkanSwapchainImage& swapchain_image) -> void;

    auto swap_simulation_objects_buffer_rw_direction() -> void;
    auto get_simulation_objects_src_buffer() -> VulkanBuffer&;
    auto get_simulation_objects_dst_buffer() -> VulkanBuffer&;
};

}
