#pragma once
#include "render_graph_pass.hpp"
#include "render_graph_pass_resources.hpp"
#include "vulkan/renderer/simulation_render_graph_passes.hpp"

namespace pop::vulkan::renderer::render_graph {

class ExecutionPlanNode {
public:
    ExecutionPlanNode() = default;
    virtual ~ExecutionPlanNode() = default;

    virtual void execute(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const PassResources& resources) = 0;
};

class PipelineBarrierNode : public ExecutionPlanNode {
public:
    PipelineBarrierNode() = default;
    ~PipelineBarrierNode() override = default;

    void execute(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const PassResources& resources) override;

private:
    std::unordered_map<BufferResourceIdentifier, vk::BufferMemoryBarrier2> m_buffer_barriers;
    std::unordered_map<ImageResourceIdentifier, vk::ImageMemoryBarrier2> m_image_barriers;
};

class PassNode : public ExecutionPlanNode {
    PassNode() = default;
    ~PassNode() override = default;

    void execute(vk::raii::CommandBuffer& cmd, const SimulationRenderState& state, const PassResources& resources) override;
private:
    PassBase<SimulationRenderState>* m_pass;
};

}