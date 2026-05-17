#pragma once
#include "mesh_pool.hpp"
#include "render_graph/render_graph_v2.hpp"
#include "render_targets.hpp"
#include "simulation_buffers.hpp"
#include "simulation_render_graph_passes.hpp"
#include "vulkan/vk_buffer.hpp"
#include "vulkan/vk_image.hpp"
#include "vulkan/vk_prelude.hpp"
#include "vulkan/vk_swapchain.hpp"
#include <cstdint>

struct ImDrawData;

namespace pop::vulkan::renderer {

constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;
constexpr uint64_t DEFAULT_GPU_DRIVEN_SIM_OBJECT_COUNT = 40000;

enum class RenderResult {
    Ok,
    SwapchainSuboptimal,
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
        VulkanSwapchain&& swapchain, render_graph::RenderGraphV2<SimulationRenderState>&& render_graph, render_graph::PassIndexV2 mesh_upload_pass_index,
        SimulationBuffersManager&& simulation_buffers_manager, RenderTargetsManager&& render_targets_manager, std::vector<FrameInFlight>&& frames_in_flight);
    ~VulkanRenderer();

    static auto create(VulkanSwapchain&& swapchain) -> VulkanRenderer;

    auto render_frame(MeshPool& mesh_pool, const std::span<const Mesh>& meshes, ImDrawData* draw_data, float delta_time, glm::vec3 cam_pos) -> RenderResult;
    auto handle_surface_invalidation(vk::Extent2D new_window_extent) -> void;
    auto swapchain() const -> const VulkanSwapchain&;

    auto reset_simulation_object_count(uint32_t new_count) -> void;

private:
    VulkanSwapchain m_swapchain;

    render_graph::RenderGraphV2<SimulationRenderState> m_render_graph;
    render_graph::PassIndexV2 m_mesh_upload_pass_index;

    SimulationBuffersManager m_simulation_buffers_manager;
    RenderTargetsManager m_render_targets_manager;


    std::vector<FrameInFlight> m_frames_in_flight;
    size_t m_current_frame = 0;

    uint32_t m_gpu_driven_sim_object_count = DEFAULT_GPU_DRIVEN_SIM_OBJECT_COUNT;
    bool m_gpu_driven_sim_needs_preinit = true;
    bool m_gpu_driven_sim_needs_refit = false; // initially prefitted to DEFAULT_GPU_DRIVEN_SIM_OBJECT_COUNT

    std::chrono::time_point<std::chrono::high_resolution_clock> m_start_timepoint = std::chrono::high_resolution_clock::now();

    auto preinitialize_simulation(const std::span<const Mesh>& meshes) -> void;
};

}
