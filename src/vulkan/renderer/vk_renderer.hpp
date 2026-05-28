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
constexpr uint64_t DEFAULT_GPU_DRIVEN_SIM_OBJECT_COUNT = 100000;
constexpr float DEFAULT_GPU_DRIVEN_SIM_WATER_CURRENT_STRENGTH = 40.0f;

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

};

struct SimulationDataSnapshot {
    std::vector<shaders::SimulationObject> simulation_objects;
    std::vector<uint32_t> simulation_objects_flags;
};

class VulkanRenderer {
public:
    VulkanRenderer(
        VulkanSwapchain&& swapchain, render_graph::RenderGraphV2<SimulationRenderState>&& render_graph, render_graph::PassIndexV2 mesh_upload_pass_index,
        render_graph::PassIndexV2 simulation_step_pass_index, render_graph::PassIndexV2 simulation_influence_step_pass_index,
        render_graph::PassIndexV2 acceleration_grid_prepare_pass_index, render_graph::PassIndexV2 acceleration_grid_radix_sort_pass_index,
        render_graph::PassIndexV2 acceleration_grid_bound_scan_pass_index, render_graph::PassIndexV2 random_events_pass_index,
        SimulationBuffersManager&& simulation_buffers_manager, RenderTargetsManager&& render_targets_manager, std::vector<FrameInFlight>&& frames_in_flight);
    ~VulkanRenderer();

    static auto create(VulkanSwapchain&& swapchain) -> VulkanRenderer;

    auto render_frame(MeshPool& mesh_pool, const std::span<const Mesh>& meshes, ImDrawData* draw_data, float delta_time, glm::vec3 cam_pos) -> RenderResult;
    auto handle_surface_invalidation(vk::Extent2D new_window_extent) -> void;
    auto swapchain() const -> const VulkanSwapchain&;

    auto reset_simulation_object_count(uint32_t new_count) -> void;
    auto set_water_current_strength(float new_strength) -> void;

    auto export_simulation_data() -> SimulationDataSnapshot;

    auto pause_simulation() -> void;
    auto resume_simulation() -> void;

    constexpr auto is_simulation_running() const -> bool { return m_simulation_is_running; }
    constexpr auto gpu_driven_sim_object_count() const -> uint32_t { return m_gpu_driven_sim_object_count; }
    constexpr auto water_current_strength() const -> float { return m_water_current_strength; }

private:
    VulkanSwapchain m_swapchain;

    render_graph::RenderGraphV2<SimulationRenderState> m_render_graph;
    render_graph::PassIndexV2 m_mesh_upload_pass_index;

    render_graph::PassIndexV2 m_simulation_step_pass_index;
    render_graph::PassIndexV2 m_simulation_influence_step_pass_index;

    render_graph::PassIndexV2 m_acceleration_grid_prepare_pass_index;
    render_graph::PassIndexV2 m_acceleration_grid_radix_sort_pass_index;
    render_graph::PassIndexV2 m_acceleration_grid_bound_scan_pass_index;

    render_graph::PassIndexV2 m_random_events_pass_index;


    SimulationBuffersManager m_simulation_buffers_manager;
    RenderTargetsManager m_render_targets_manager;


    std::vector<FrameInFlight> m_frames_in_flight;
    size_t m_current_frame = 0;

    uint32_t m_gpu_driven_sim_object_count = DEFAULT_GPU_DRIVEN_SIM_OBJECT_COUNT;
    bool m_gpu_driven_sim_needs_preinit = true;
    bool m_gpu_driven_sim_needs_refit = false; // initially prefitted to DEFAULT_GPU_DRIVEN_SIM_OBJECT_COUNT
    float m_water_current_strength = DEFAULT_GPU_DRIVEN_SIM_WATER_CURRENT_STRENGTH;

    std::chrono::time_point<std::chrono::high_resolution_clock> m_start_timepoint = std::chrono::high_resolution_clock::now();
    std::chrono::time_point<std::chrono::high_resolution_clock> m_last_random_events_timepoint = std::chrono::high_resolution_clock::now();

    bool m_simulation_is_running = true;
    std::chrono::high_resolution_clock::duration m_time_from_random_events_when_paused = std::chrono::high_resolution_clock::duration::zero();

    auto preinitialize_simulation(const std::span<const Mesh>& meshes) -> void;
};

}
