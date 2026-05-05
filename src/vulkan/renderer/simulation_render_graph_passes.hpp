#pragma once
#include "../../../cmake-build-debug/_deps/imgui-src/imgui.h"
#include "mesh_pool.hpp"
#include "vulkan/renderer/render_graph/render_graph_pass.hpp"
#include "vulkan/vk_compute_pipeline.hpp"
#include "vulkan/vk_pipeline_layout.hpp"
#include "vulkan/vk_swapchain.hpp"

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

struct SimulationRenderState {
    std::reference_wrapper<MeshPool> mesh_pool;
    std::reference_wrapper<VulkanSwapchainImage> current_swapchain_image;
    uint32_t object_count;
    ImDrawData* imgui_draw_data;

};

// ---- SimulationUploadMeshInfoPass ---------------------------------------------------------------------------------------------------------------------------

class UploadMeshInfoPass : public render_graph::PassBase<SimulationRenderState> {
public:
    UploadMeshInfoPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout, VulkanComputePipeline&& compute_pipeline);

    static auto create() -> UploadMeshInfoPass;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_pipeline_layout;
    VulkanComputePipeline m_compute_pipeline;
};

// ---- SimulationIndirectDrawCommandsResetPass ----------------------------------------------------------------------------------------------------------------

class IndirectDrawCommandsClearPass : public render_graph::PassBase<SimulationRenderState> {
public:
    IndirectDrawCommandsClearPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout, VulkanComputePipeline&& compute_pipeline);

    static auto create() -> IndirectDrawCommandsClearPass;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_pipeline_layout;
    VulkanComputePipeline m_compute_pipeline;
};

// ---- SimulationStepPass -------------------------------------------------------------------------------------------------------------------------------------

class SimulationStepPass : public render_graph::PassBase<SimulationRenderState> {
public:
    SimulationStepPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout, VulkanComputePipeline&& compute_pipeline);

    static auto create() -> SimulationStepPass;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_pipeline_layout;
    VulkanComputePipeline m_compute_pipeline;
};

// ---- IndirectDrawCommandsInstanceCountBuildPass -------------------------------------------------------------------------------------------------------------

class IndirectDrawCommandsInstanceCountBuildPass : public render_graph::PassBase<SimulationRenderState> {
public:
    IndirectDrawCommandsInstanceCountBuildPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout, VulkanComputePipeline&& compute_pipeline);

    static auto create() -> IndirectDrawCommandsInstanceCountBuildPass;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_pipeline_layout;
    VulkanComputePipeline m_compute_pipeline;
};

// ---- IndirectDrawCommandsFirstInstanceBuildPass -------------------------------------------------------------------------------------------------------------

class IndirectDrawCommandsFirstInstanceBuildPass : public render_graph::PassBase<SimulationRenderState> {
public:
    IndirectDrawCommandsFirstInstanceBuildPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout, VulkanComputePipeline&& compute_pipeline);

    static auto create() -> IndirectDrawCommandsFirstInstanceBuildPass;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_pipeline_layout;
    VulkanComputePipeline m_compute_pipeline;
};

// ---- InstanceBufferBuildPass --------------------------------------------------------------------------------------------------------------------------------

class InstanceBufferBuildPass : public render_graph::PassBase<SimulationRenderState> {
public:
    InstanceBufferBuildPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout, VulkanComputePipeline&& compute_pipeline);

    static auto create() -> InstanceBufferBuildPass;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_pipeline_layout;
    VulkanComputePipeline m_compute_pipeline;
};

// ---- FishTankRenderPass -------------------------------------------------------------------------------------------------------------------------------------

class FishTankRenderPass : public render_graph::PassBase<SimulationRenderState> {
public:
    FishTankRenderPass(render_graph::PassDependencies&& deps, VulkanPipelineLayout&& pipeline_layout, VulkanGraphicsPipeline&& graphics_pipeline);

    static auto create() -> FishTankRenderPass;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const render_graph::PassResources& resources) -> void override;

private:
    VulkanPipelineLayout m_pipeline_layout;
    VulkanGraphicsPipeline m_graphics_pipeline;
};

// ---- ImGuiRenderPass ---------------------------------------------------------------------------------------------------------------------------------------

class ImGuiRenderPass : public render_graph::PassBase<SimulationRenderState> {
public:
    ImGuiRenderPass(render_graph::PassDependencies&& deps);

    static auto create() -> ImGuiRenderPass;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const render_graph::PassResources& resources) -> void override;
};

// ---- BlitMainImageToSwapchainPass ---------------------------------------------------------------------------------------------------------------------------

class BlitMainImageToSwapchainPass : public render_graph::PassBase<SimulationRenderState> {
public:
    BlitMainImageToSwapchainPass(render_graph::PassDependencies&& deps);

    static auto create() -> BlitMainImageToSwapchainPass;

    auto invoke(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const render_graph::PassResources& resources) -> void override;
};

}