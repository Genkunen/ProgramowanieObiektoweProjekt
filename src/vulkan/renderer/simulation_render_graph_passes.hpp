#pragma once
#include "mesh_pool.hpp"
#include "vulkan/renderer/render_graph/render_graph_pass.hpp"
#include "vulkan/vk_compute_pipeline.hpp"
#include "vulkan/vk_pipeline_layout.hpp"
#include "vulkan/vk_swapchain.hpp"
#include "systems/ktx2_loader.hpp"
#include <imgui.h>

namespace pop::vulkan::renderer {

inline uint32_t div_ceil(uint32_t a, uint32_t b) {
    return (a + b - 1) / b;
}

inline vk::Offset3D to_offset3d(const vk::Extent3D& extent) {
    return vk::Offset3D{
        static_cast<int32_t>(extent.width),
        static_cast<int32_t>(extent.height),
        static_cast<int32_t>(extent.depth)
    };
}

/// @struct SimulationSharedPassData
/// @brief Shared data for all simulation render graph passes.
struct SimulationSharedPassData {
    std::reference_wrapper<MeshPool> mesh_pool;
    std::reference_wrapper<const VulkanSwapchainImage> current_swapchain_image;
    ImDrawData* imgui_draw_data;
    glm::vec2 simulation_bounds;
    uint32_t object_count;
    float grid_cell_size;
    uint32_t grid_width;
    uint32_t grid_height;
    float water_current_strength;
};

// ---- UploadMeshParamsPass -----------------------------------------------------------------------------------------------------------------------------------

/// @class UploadMeshParamsPass
/// @brief (Render Graph Pass) Uploads mesh parameters to a respective @c vk::DrawIndexedIndirectCommand in GPU memory.
/// @details This pass dispatches a compute shader that reads meshes' @c first_index, @c index_count, and @c vertex_offset parameters, and writes them to the
///     respective @c vk::DrawIndexedIndirectCommand instance in the indirect draw commands buffer for the indirect draw handling instances of that mesh.
class UploadMeshParamsPass : public render_graph::PassBase<SimulationSharedPassData> {
public:
    UploadMeshParamsPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout, VulkanComputePipeline&& compute_pipeline);

    static auto create() -> UploadMeshParamsPass;

    auto debug_name() const noexcept -> std::string override;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationSharedPassData& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_pipeline_layout;
    VulkanComputePipeline m_compute_pipeline;
};

// ---- RandomEventsPass ---------------------------------------------------------------------------------------------------------------------------------------

/// @class RandomEventsPass
/// @brief (Render Graph Pass) Generates random events for the simulation.
/// @details This pass dispatches a compute shader that randomly resets a number of objects back from a dead state into a randomly chosen object type.
class RandomEventsPass : public render_graph::PassBase<SimulationSharedPassData> {
public:
    RandomEventsPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout, VulkanComputePipeline&& compute_pipeline);

    static auto create() -> RandomEventsPass;

    auto debug_name() const noexcept -> std::string override;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationSharedPassData& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_pipeline_layout;
    VulkanComputePipeline m_compute_pipeline;
};

// ---- IndirectDrawCommandsInstanceCountClearPass -------------------------------------------------------------------------------------------------------------

/// @class IndirectDrawCommandsInstanceCountClearPass
/// @brief (Render Graph Pass) Clears out the @c instanceCount parameters in all @c vk::DrawIndexedIndirectCommand instances in the indirect draw commands
///     buffer using a compute shader.
class IndirectDrawCommandsInstanceCountClearPass : public render_graph::PassBase<SimulationSharedPassData> {
public:
    IndirectDrawCommandsInstanceCountClearPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout, VulkanComputePipeline&& compute_pipeline);

    static auto create() -> IndirectDrawCommandsInstanceCountClearPass;

    auto debug_name() const noexcept -> std::string override;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationSharedPassData& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_pipeline_layout;
    VulkanComputePipeline m_compute_pipeline;
};

// ---- SimulationInternalStepPass -----------------------------------------------------------------------------------------------------------------------------

/// @class SimulationInternalStepPass
/// @brief (Render Graph Pass) Performs an internal simulation step for all live objects in the simulation using a compute shader.
class SimulationInternalStepPass : public render_graph::PassBase<SimulationSharedPassData> {
public:
    SimulationInternalStepPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout, VulkanComputePipeline&& compute_pipeline);

    static auto create() -> SimulationInternalStepPass;

    auto debug_name() const noexcept -> std::string override;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationSharedPassData& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_pipeline_layout;
    VulkanComputePipeline m_compute_pipeline;
};

// ---- SimulationAccelerationGridSortPreparePass --------------------------------------------------------------------------------------------------------------

/// @class SimulationAccelerationGridSortPreparePass
/// @brief (Render Graph Pass) Prepares sort data to build a spatial hash grid based on object locations.
/// @details Dispatches a compute shader to fill the acceleration grid sort key and value buffers with a position-based tile index for a given object and the
///     index of that object, respectively.
class SimulationAccelerationGridSortPreparePass : public render_graph::PassBase<SimulationSharedPassData> {
public:
    SimulationAccelerationGridSortPreparePass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout, VulkanComputePipeline&& compute_pipeline);

    static auto create() -> SimulationAccelerationGridSortPreparePass;

    auto debug_name() const noexcept -> std::string override;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationSharedPassData& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_pipeline_layout;
    VulkanComputePipeline m_compute_pipeline;
};

// ---- SimulationAccelerationGridRadixSortPass ----------------------------------------------------------------------------------------------------------------

/// @class SimulationAccelerationGridRadixSortPass
/// @brief (Render Graph Pass) Sorts the key and value buffers prepared by @c SimulationAccelerationGridSortPreparePass to form the spatial hash grid.
/// @details Dispatches a number of passes of a set of compute shaders to run a radix sort algorithm on the key and value buffers prepared by
///     @c SimulationAccelerationGridSortPreparePass to form a spatial hash grid. After the sort is completed, the key buffer holds a monotonically increasing
///     set of spatial hashes joined with object IDs in the value buffer. The indices at which the spatial hash values in the key buffer increase are then found
///     by @c SimulationAccelerationGridBoundScanPass.
class SimulationAccelerationGridRadixSortPass : public render_graph::PassBase<SimulationSharedPassData> {
public:
    SimulationAccelerationGridRadixSortPass(render_graph::PassDependencies&& deps,
        VulkanPipelineLayout&& histogram_pass_pipeline_layout, VulkanComputePipeline&& histogram_pass_compute_pipeline,
        VulkanPipelineLayout&& column_prefix_sum_pass_pipeline_layout, VulkanComputePipeline&& column_prefix_sum_pass_compute_pipeline,
        VulkanPipelineLayout&& global_prefix_sum_pass_pipeline_layout, VulkanComputePipeline&& global_prefix_sum_pass_compute_pipeline,
        VulkanPipelineLayout&& scatter_pass_pipeline_layout, VulkanComputePipeline&& scatter_pass_compute_pipeline
    );

    static auto create() -> SimulationAccelerationGridRadixSortPass;

    auto debug_name() const noexcept -> std::string override;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationSharedPassData& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_histogram_pass_pipeline_layout;
    VulkanComputePipeline m_histogram_pass_compute_pipeline;

    VulkanPipelineLayout m_column_prefix_sum_pass_pipeline_layout;
    VulkanComputePipeline m_column_prefix_sum_pass_compute_pipeline;

    VulkanPipelineLayout m_global_prefix_sum_pass_pipeline_layout;
    VulkanComputePipeline m_global_prefix_sum_pass_compute_pipeline;

    VulkanPipelineLayout m_scatter_pass_pipeline_layout;
    VulkanComputePipeline m_scatter_pass_compute_pipeline;
};

// ---- SimulationAccelerationGridBoundClearPass ---------------------------------------------------------------------------------------------------------------

/// @class SimulationAccelerationGridBoundClearPass
/// @brief (Render Graph Pass) Dispatches a transfer operation to default-initialize the acceleration grid spatial hash boundary indices.
class SimulationAccelerationGridBoundClearPass : public render_graph::PassBase<SimulationSharedPassData> {
public:
    SimulationAccelerationGridBoundClearPass(render_graph::PassDependencies&& deps);

    static auto create() -> SimulationAccelerationGridBoundClearPass;

    auto debug_name() const noexcept -> std::string override;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationSharedPassData& state, const render_graph::PassResources& resources) -> void override;
};

// ---- SimulationAccelerationGridBoundScanPass ----------------------------------------------------------------------------------------------------------------

/// @class SimulationAccelerationGridBoundScanPass
/// @brief (Render Graph Pass) Dispatches a compute shader to scan the spatial hash grid boundary indices to find the indices of the first and last objects in
///     each spatial hash grid cell. Stores results to the acceleration grid spatial hash boundary start/end buffers.
class SimulationAccelerationGridBoundScanPass : public render_graph::PassBase<SimulationSharedPassData> {
public:
    SimulationAccelerationGridBoundScanPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout, VulkanComputePipeline&& compute_pipeline);

    static auto create() -> SimulationAccelerationGridBoundScanPass;

    auto debug_name() const noexcept -> std::string override;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationSharedPassData& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_pipeline_layout;
    VulkanComputePipeline m_compute_pipeline;
};

// ---- SimulationInfluenceStepPass ----------------------------------------------------------------------------------------------------------------------------

/// @class SimulationInfluenceStepPass
/// @brief (Render Graph Pass) Performs a simulation step where objects simulate interactions with one another using a compute shader.
class SimulationInfluenceStepPass : public render_graph::PassBase<SimulationSharedPassData> {
public:
    SimulationInfluenceStepPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout, VulkanComputePipeline&& compute_pipeline);

    static auto create() -> SimulationInfluenceStepPass;

    auto debug_name() const noexcept -> std::string override;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationSharedPassData& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_pipeline_layout;
    VulkanComputePipeline m_compute_pipeline;
};

// ---- IndirectDrawCommandsInstanceCountBuildPass -------------------------------------------------------------------------------------------------------------

/// @class IndirectDrawCommandsInstanceCountBuildPass
/// @brief (Render Graph Pass) Builds the @c instanceCount parameters in all @c vk::DrawIndexedIndirectCommand instances in the indirect draw commands buffer
///     using a compute shader based on all live objects.
class IndirectDrawCommandsInstanceCountBuildPass : public render_graph::PassBase<SimulationSharedPassData> {
public:
    IndirectDrawCommandsInstanceCountBuildPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout, VulkanComputePipeline&& compute_pipeline);

    static auto create() -> IndirectDrawCommandsInstanceCountBuildPass;

    auto debug_name() const noexcept -> std::string override;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationSharedPassData& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_pipeline_layout;
    VulkanComputePipeline m_compute_pipeline;
};

// ---- IndirectDrawCommandsFirstInstanceBuildPass -------------------------------------------------------------------------------------------------------------

/// @class IndirectDrawCommandsFirstInstanceBuildPass
/// @brief (Render Graph Pass) Builds the @c firstInstance parameters in all @c vk::DrawIndexedIndirectCommand instances in the indirect draw commands buffer
///     using a compute shader based on an exclusive prefix sum of @c instanceCount parameters built by @c IndirectDrawCommandsInstanceCountBuildPass.
class IndirectDrawCommandsFirstInstanceBuildPass : public render_graph::PassBase<SimulationSharedPassData> {
public:
    IndirectDrawCommandsFirstInstanceBuildPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout, VulkanComputePipeline&& compute_pipeline);

    static auto create() -> IndirectDrawCommandsFirstInstanceBuildPass;

    auto debug_name() const noexcept -> std::string override;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationSharedPassData& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_pipeline_layout;
    VulkanComputePipeline m_compute_pipeline;
};

// ---- InstanceBufferBuildPass --------------------------------------------------------------------------------------------------------------------------------

/// @class InstanceBufferBuildPass
/// @brief (Render Graph Pass) Builds the object instance buffer for all live objects in the simulation using a compute shader to use in rendering.
class InstanceBufferBuildPass : public render_graph::PassBase<SimulationSharedPassData> {
public:
    InstanceBufferBuildPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout, VulkanComputePipeline&& compute_pipeline);

    static auto create() -> InstanceBufferBuildPass;

    auto debug_name() const noexcept -> std::string override;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationSharedPassData& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_pipeline_layout;
    VulkanComputePipeline m_compute_pipeline;
};

// ---- BackgroundRenderPass -----------------------------------------------------------------------------------------------------------------------------------

/// @class BackgroundRenderPass
/// @brief (Render Graph Pass) Renders the background of the simulation using a graphics pipeline.
class BackgroundRenderPass : public render_graph::PassBase<SimulationSharedPassData> {
public:
    BackgroundRenderPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout, VulkanGraphicsPipeline&& graphics_pipeline);

    static auto create() -> BackgroundRenderPass;

    auto debug_name() const noexcept -> std::string override;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationSharedPassData& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_pipeline_layout;
    VulkanGraphicsPipeline m_graphics_pipeline;
};

// ---- FishTankRenderPass -------------------------------------------------------------------------------------------------------------------------------------

/// @class FishTankRenderPass
/// @brief (Render Graph Pass) Renders all live objects using a graphics pipeline, based on the object instance buffer built by @c InstanceBufferBuildPass.
class FishTankRenderPass : public render_graph::PassBase<SimulationSharedPassData> {
public:
    FishTankRenderPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout, VulkanGraphicsPipeline&& graphics_pipeline,
                       vk::raii::Sampler&& sampler, pop::systems::Ktx2Loader&& loader, vk::raii::DescriptorPool&& pool, vk::raii::DescriptorSet&& set,
                       VulkanImage&& fish_texture, VulkanImage&& food_texture, VulkanImage&& predator_texture);

    static auto create() -> FishTankRenderPass;

    auto debug_name() const noexcept -> std::string override;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationSharedPassData& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_pipeline_layout;
    VulkanGraphicsPipeline m_graphics_pipeline;
    vk::raii::Sampler m_sampler;
    pop::systems::Ktx2Loader m_texture_loader;
    vk::raii::DescriptorPool m_descriptor_pool;
    vk::raii::DescriptorSet m_descriptor_set;
    VulkanImage m_fish_texture;
    VulkanImage m_food_texture;
    VulkanImage m_predator_texture;
};

// ---- ImGuiRenderPass ---------------------------------------------------------------------------------------------------------------------------------------

/// @class ImGuiRenderPass
/// @brief (Render Graph Pass) Renders the ImGui interface.
class ImGuiRenderPass : public render_graph::PassBase<SimulationSharedPassData> {
public:
    ImGuiRenderPass(render_graph::PassDependencies&& deps);

    static auto create() -> ImGuiRenderPass;

    auto debug_name() const noexcept -> std::string override;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationSharedPassData& state, const render_graph::PassResources& resources) -> void override;
};

// ---- BlitMainImageToSwapchainPass ---------------------------------------------------------------------------------------------------------------------------

/// @class BlitMainImageToSwapchainPass
/// @brief (Render Graph Pass) Blit the main render target to the swapchain image currently in use by the application.
class BlitMainImageToSwapchainPass : public render_graph::PassBase<SimulationSharedPassData> {
public:
    BlitMainImageToSwapchainPass(render_graph::PassDependencies&& deps);

    static auto create() -> BlitMainImageToSwapchainPass;

    auto debug_name() const noexcept -> std::string override;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationSharedPassData& state, const render_graph::PassResources& resources) -> void override;
};

} // namespace pop::vulkan::renderer